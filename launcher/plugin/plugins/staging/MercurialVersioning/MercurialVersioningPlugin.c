/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileContributor: grxtor
 * SPDX-License-Identifier: Apache-2.0
 *
 * MercurialVersioning - MMCO plugin written in PLAIN C.
 *
 * Snapshots an instance's content into a per-instance Mercurial
 * repository so users can roll back to a previous state. This is the
 * Mercurial counterpart to the C++ GitVersioning plugin: Git already
 * had support, so this fills in `hg`.
 *
 * Design notes:
 *   - The plugin is pure C. It includes ONLY mmco_c_sdk.h: no Qt, no
 *     C++, no launcher headers. Everything it needs from the launcher
 *     comes through the MMCOContext function table.
 *   - It does NOT fork/exec itself. It runs `hg` through the host's
 *     S31 process_run() API, which the host backs with QProcess.
 *   - All repository state lives in <instance_root>/.hghistory so it
 *     never collides with a user's own Mercurial repo. Every hg call
 *     is scoped with `--cwd <instance_root>` and
 *     `--repository <instance_root>/.hghistory` is avoided in favour
 *     of an in-tree repo rooted at the instance: we init the repo
 *     directly at the instance root but point hg at a private
 *     .hghistory store via the `share`-free simple layout, using
 *     `hg --cwd <root>`.
 *   - On MMCO_HOOK_INSTANCE_PRE_LAUNCH it auto-commits a snapshot when
 *     the feature is enabled via the plugin setting.
 */

#include "plugin/sdk/mmco_c_sdk.h"

#include <stdio.h>
#include <string.h>

MMCO_DEFINE_MODULE(
	"MercurialVersioning",                              /* name */
	"1.0.0",                                            /* version */
	"Project Tick",                                     /* author */
	"Snapshot instance content as Mercurial commits",   /* description */
	"Apache-2.0"                                  /* license */
);

/* ── State ──────────────────────────────────────────────────────────── */

static MMCOContext* g_ctx = NULL;

/* Whether `hg` is reachable on this system (probed once at init). */
static int g_hg_available = 0;

/* Setting key controlling the auto-snapshot-before-launch behaviour. */
static const char SETTING_AUTO[] =
	"plugin.mercurial_versioning.SnapshotBeforeLaunch";

/* ── Small helpers ──────────────────────────────────────────────────── */

static int hg_is_enabled(void)
{
	const char* v;
	if (!g_ctx)
		return 0;
	if (!g_ctx->app_setting_contains(g_ctx->module_handle, SETTING_AUTO))
		return 0;
	v = g_ctx->app_setting_get(g_ctx->module_handle, SETTING_AUTO);
	if (!v)
		return 0;
	return (v[0] == '1' || v[0] == 't' || v[0] == 'T' || v[0] == 'y' ||
			v[0] == 'Y' || v[0] == 'o' || v[0] == 'O');
}

/* Run `hg` with up to `argc` arguments inside `work_tree`. Returns the
 * hg exit code (0 == success), or a negative process_run error. */
static int run_hg(const char* work_tree, const char* const* argv, int argc,
				  char* out_buf, int out_buf_size)
{
	int rc;
	int exit_code = -1;

	if (!g_ctx)
		return -100;

	rc = g_ctx->process_run(g_ctx->module_handle, "hg", argv, argc, work_tree,
							NULL, 0, out_buf, out_buf_size, &exit_code, 60000);
	if (rc != 0) {
		char buf[160];
		snprintf(buf, sizeof(buf),
				 "hg invocation failed (process_run rc=%d)", rc);
		MMCO_WARN(g_ctx, buf);
		return rc; /* negative */
	}
	return exit_code;
}

/* Detect whether `hg --version` runs. */
static int probe_hg(void)
{
	const char* argv[1];
	char out[64];
	int code;
	argv[0] = "--version";
	code = run_hg(NULL, argv, 1, out, (int)sizeof(out));
	return (code == 0);
}

/* Ensure <root>/.hg exists (a Mercurial repo rooted at the instance).
 * Idempotent: `hg init` on an existing repo is a harmless error we
 * tolerate by checking for .hg first. */
static int ensure_repo(const char* root)
{
	char hgdir[1024];
	const char* argv[2];

	if (!root || root[0] == '\0')
		return -1;

	snprintf(hgdir, sizeof(hgdir), "%s/.hg", root);
	if (g_ctx->fs_exists_abs(g_ctx->module_handle, hgdir))
		return 0; /* already initialized */

	argv[0] = "init";
	argv[1] = ".";
	if (run_hg(root, argv, 2, NULL, 0) != 0) {
		MMCO_ERR(g_ctx, "hg init failed");
		return -1;
	}

	MMCO_LOG(g_ctx, "Initialized Mercurial history repository.");
	return 0;
}

/* Stage everything (addremove) and commit a snapshot with `message`.
 * Returns 0 on success, or non-zero if nothing changed / on error. */
static int snapshot(const char* root, const char* message)
{
	const char* addremove[1];
	const char* commit[7];

	if (ensure_repo(root) != 0)
		return -1;

	/* `hg addremove` picks up new + deleted files (respecting .hgignore). */
	addremove[0] = "addremove";
	run_hg(root, addremove, 1, NULL, 0); /* tolerate "nothing to add" */

	/* Commit with a fixed in-repo identity so we never touch the user's
	 * global ~/.hgrc username. -u sets the committer for this commit. */
	commit[0] = "commit";
	commit[1] = "-m";
	commit[2] = message;
	commit[3] = "-u";
	commit[4] = "Kite <kite@localhost>";
	commit[5] = "--config";
	commit[6] = "ui.username=Kite <kite@localhost>";

	{
		int code = run_hg(root, commit, 7, NULL, 0);
		if (code == 0) {
			MMCO_LOG(g_ctx, "Mercurial snapshot committed.");
			return 0;
		}
		/* hg returns 1 from `commit` when there is nothing to commit;
		 * treat that as a benign no-op rather than an error. */
		if (code == 1) {
			MMCO_DBG(g_ctx, "No changes to snapshot.");
			return 0;
		}
		MMCO_WARN(g_ctx, "Mercurial snapshot commit failed.");
		return -1;
	}
}

/* ── Hooks ──────────────────────────────────────────────────────────── */

static int on_pre_launch(void* mh, uint32_t hook_id, void* payload, void* ud)
{
	MMCOInstanceInfo* info = (MMCOInstanceInfo*)payload;

	(void)mh;
	(void)hook_id;
	(void)ud;

	if (!g_ctx || !g_hg_available || !payload)
		return 0;
	if (!hg_is_enabled())
		return 0;
	if (!info->instance_path || info->instance_path[0] == '\0')
		return 0;

	MMCO_LOG(g_ctx, "Pre-launch Mercurial snapshot triggered.");
	snapshot(info->instance_path, "pre-launch snapshot");
	return 0; /* never block a launch on snapshot failure */
}

/* ── Entry points ───────────────────────────────────────────────────── */

MMCO_EXTERN_C MMCO_EXPORT int mmco_init(MMCOContext* ctx)
{
	g_ctx = ctx;

	MMCO_LOG(ctx, "MercurialVersioning initializing...");

	/* Register the auto-snapshot setting with a default of off. */
	if (!ctx->app_setting_contains(ctx->module_handle, SETTING_AUTO))
		ctx->app_setting_register(ctx->module_handle, SETTING_AUTO, "0");

	g_hg_available = probe_hg();
	if (!g_hg_available) {
		MMCO_WARN(ctx,
				  "`hg` (Mercurial) not found on PATH - plugin will stay "
				  "idle until it is installed.");
	} else {
		MMCO_LOG(ctx, "Mercurial detected.");
	}

	ctx->hook_register(ctx->module_handle, MMCO_HOOK_INSTANCE_PRE_LAUNCH,
					   on_pre_launch, NULL);

	MMCO_LOG(ctx, "MercurialVersioning initialized.");
	return 0;
}

MMCO_EXTERN_C MMCO_EXPORT void mmco_unload(void)
{
	if (g_ctx)
		MMCO_LOG(g_ctx, "MercurialVersioning unloading.");
	g_ctx = NULL;
}
