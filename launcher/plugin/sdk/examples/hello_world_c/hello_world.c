/* SPDX-FileCopyrightText: Project Tick
 * SPDX-FileContributor: Project Tick
 * SPDX-FileContributor: grxtor
 * SPDX-License-Identifier: CC0-1.0
 *
 * Example Kite plugin (.mmco module) written in PLAIN C.
 *
 * This demonstrates that a .mmco module can be authored in C with no
 * Qt and no C++ at all: it includes only the language-neutral C ABI
 * header (mmco_c_sdk.h) and links against the Qt-free Kite::SDK_C
 * target. A C plugin gets the full data-plane API (logging, hooks,
 * instances, settings, …) but not the Qt UI page builder, which is
 * only meaningful from C++.
 */

#include "plugin/sdk/mmco_c_sdk.h"
#include <stdio.h>

/* Module declaration. Note the trailing ';' — MMCO_DEFINE_MODULE
 * expands to a single declaration, so it is terminated like any other. */
MMCO_DEFINE_MODULE(
	"Hello World (C)",                            /* name */
	"1.0.0",                                      /* version */
	"Project Tick",                               /* author */
	"Example .mmco written in plain C",           /* description */
	"CC0-1.0"                                     /* license */
);

/* State */
static MMCOContext* g_ctx = NULL;

/* Hook callback: app finished initializing. */
static int on_app_initialized(void* mh, uint32_t hook_id, void* payload,
							  void* user_data)
{
	int count;
	int i;
	char buf[256];

	(void)mh;
	(void)hook_id;
	(void)payload;
	(void)user_data;

	MMCO_LOG(g_ctx, "Hello from the plain-C Hello World plugin!");

	count = g_ctx->instance_count(g_ctx->module_handle);
	snprintf(buf, sizeof(buf), "Kite has %d instance(s):", count);
	MMCO_LOG(g_ctx, buf);

	for (i = 0; i < count; ++i) {
		const char* id = g_ctx->instance_get_id(g_ctx->module_handle, i);
		if (id) {
			const char* name =
				g_ctx->instance_get_name(g_ctx->module_handle, id);
			snprintf(buf, sizeof(buf), "  [%d] %s (%s)", i,
					 name ? name : "?", id);
			MMCO_LOG(g_ctx, buf);
		}
	}

	/* Demonstrate the (namespaced) settings API. */
	g_ctx->setting_set(g_ctx->module_handle, "last_run", "just now");

	return 0; /* continue hook chain */
}

/* Hook callback: an instance is about to launch. */
static int on_instance_pre_launch(void* mh, uint32_t hook_id, void* payload,
								  void* user_data)
{
	MMCOInstanceInfo* info = (MMCOInstanceInfo*)payload;

	(void)mh;
	(void)hook_id;
	(void)user_data;

	if (info && info->instance_name) {
		char buf[256];
		snprintf(buf, sizeof(buf), "Instance '%s' is about to launch!",
				 info->instance_name);
		MMCO_LOG(g_ctx, buf);
	}
	return 0;
}

/* Entry points — exported with C linkage and default visibility. */

MMCO_EXTERN_C MMCO_EXPORT int mmco_init(MMCOContext* ctx)
{
	g_ctx = ctx;

	MMCO_LOG(ctx, "Hello World (C) plugin initializing...");

	ctx->hook_register(ctx->module_handle, MMCO_HOOK_APP_INITIALIZED,
					   on_app_initialized, NULL);
	ctx->hook_register(ctx->module_handle, MMCO_HOOK_INSTANCE_PRE_LAUNCH,
					   on_instance_pre_launch, NULL);

	MMCO_LOG(ctx, "Hello World (C) plugin initialized.");
	return 0;
}

MMCO_EXTERN_C MMCO_EXPORT void mmco_unload(void)
{
	if (g_ctx) {
		MMCO_LOG(g_ctx, "Hello World (C) plugin unloading. Goodbye!");
	}
	g_ctx = NULL;
}
