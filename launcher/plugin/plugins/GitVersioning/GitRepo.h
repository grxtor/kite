/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileContributor: grxtor
 * SPDX-License-Identifier: Apache-2.0
 *
 * GitRepo — thin wrapper around `git` (the binary) for the
 * GitVersioning plugin.
 *
 * We deliberately do NOT link against libgit2 — it would be a heavy
 * extra dependency for the one-shot commands we actually need. Modern
 * desktops always ship a `git` binary, and Windows users who installed
 * Kite almost certainly also installed Git for Windows. The plugin
 * detects a missing `git` at init time and disables itself cleanly.
 *
 * All operations live inside the instance's own `.history/` directory
 * which holds a bare repository plus a working tree that is the
 * instance root (--git-dir / --work-tree mode). The plugin never
 * touches the user's HOME .gitconfig and configures a local
 * author identity scoped to the instance.
 */

#pragma once

#include "plugin/sdk/mmco_cxx_sdk.hpp"
#include <QObject>
#include <QProcess>

struct GitCommit {
	QString sha;	 // short sha (8 chars)
	QString fullSha; // full 40-char sha
	QString subject; // commit subject (single line)
	QString author;
	QDateTime when;
	qint64 sizeAdded = 0;	// +bytes (best-effort, from --shortstat)
	qint64 sizeRemoved = 0; // -bytes
	int filesChanged = 0;
	bool isPreLaunch = false; // commit was created by the pre-launch hook
};

struct GitRepoStatus {
	bool initialized = false;
	bool dirty = false; // working tree has changes
	int untrackedCount = 0;
	int modifiedCount = 0;
	int deletedCount = 0;
	QString head; // current short sha or "(empty)"
};

class GitRepo
{
  public:
	GitRepo(const QString& instanceId, const QString& instanceRoot);

	/* True if the system `git` binary is reachable. */
	static bool gitAvailable();
	static QString gitVersion();

	bool isInitialized() const;
	QString repoDir() const
	{
		return m_repoDir;
	}
	QString workTree() const
	{
		return m_instanceRoot;
	}

	/* Idempotent: creates .history/, runs `git init --bare`, writes a
	 * sensible .gitignore for the work-tree, and pins the local author
	 * identity. Returns false on failure. */
	bool initialize(QString* errorMsg = nullptr);

	GitRepoStatus status() const;

	/* Stage every tracked + untracked path (respecting .gitignore) and
	 * commit with the given message. Returns the new short sha, or an
	 * empty string if nothing changed. */
	QString commit(const QString& message, bool isPreLaunch = false,
				   QString* errorMsg = nullptr);

	/* History, newest first, capped at `limit`. */
	QList<GitCommit> log(int limit = 200) const;

	/* git diff --shortstat <sha>~..<sha> — fills sizeAdded/sizeRemoved
	 * on the entry. Cheap, used to populate the UI on demand. */
	void fillCommitStats(GitCommit& c) const;

	/* Restore the entire work tree to the state at `sha`. Internally
	 * does `git checkout <sha> -- .` then `git reset --mixed HEAD`,
	 * so HEAD stays where it was — restore is reversible by another
	 * `restore` to a later commit. Returns true on success. */
	bool restore(const QString& sha, QString* errorMsg = nullptr);

	/* Resolve `name` (branch / tag / shorthand sha) to a full sha, or
	 * return an empty string if unknown. */
	QString resolveRef(const QString& name) const;

	/* Read a single file's contents at the given revision, into a
	 * QByteArray. Returns an empty QByteArray + sets ok=false on
	 * failure. */
	QByteArray showFile(const QString& sha, const QString& path,
						bool* ok = nullptr) const;

	/* Make a tag at the given commit (or HEAD if sha empty). */
	bool tag(const QString& name, const QString& sha = {},
			 QString* errorMsg = nullptr);

	/* Delete a commit by hard-resetting to its parent. Only allowed
	 * for the HEAD commit; older commits are immutable to keep the
	 * history coherent. Returns false otherwise. */
	bool dropHead(QString* errorMsg = nullptr);

  private:
	struct GitResult {
		int exitCode = -1;
		QByteArray stdoutBytes;
		QByteArray stderrBytes;
	};

	GitResult runGit(const QStringList& args, int timeoutMs = 30000) const;
	void writeGitIgnore() const;
	void writeRepoConfig() const;

	QString m_instanceId;
	QString m_instanceRoot;
	QString m_repoDir; // <instanceRoot>/.history
};
