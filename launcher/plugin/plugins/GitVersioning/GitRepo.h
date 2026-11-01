/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileContributor: grxtor
 * SPDX-License-Identifier: Apache-2.0
 *
 * GitRepo: thin wrapper around the `git` binary for the GitVersioning plugin.
 *
 * libgit2 is not linked: it would be a heavy dependency for a few one-shot commands, and desktops
 * (and Windows users with Git for Windows) have `git`. A missing `git` is detected at init and the
 * plugin disables itself.
 *
 * Everything lives in the instance's `.history/` directory used as the git-dir, with the instance
 * root as the work tree (--git-dir / --work-tree). The user's HOME .gitconfig is never touched; a
 * local author identity is configured per instance.
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

	/* Idempotent: creates .history/ (a non-bare git-dir), writes a .gitignore for the work tree and
	 * pins the local author identity. False on failure. */
	bool initialize(QString* errorMsg = nullptr);

	GitRepoStatus status() const;

	/* Stages every tracked and untracked path (respecting .gitignore) and commits; returns the new
	 * short sha, or empty if nothing changed. */
	QString commit(const QString& message, bool isPreLaunch = false,
				   QString* errorMsg = nullptr);

	/* History, newest first, capped at `limit`. */
	QList<GitCommit> log(int limit = 200) const;

	/* git diff --shortstat <sha>~..<sha>; fills sizeAdded/sizeRemoved on the entry. Cheap, for
	 * on-demand UI. */
	void fillCommitStats(GitCommit& c) const;

	/* Restores the whole work tree to `sha` (`git checkout <sha> -- .` then `git reset --mixed
	 * HEAD`). HEAD does not move, so a restore is reversible by restoring a later commit. */
	bool restore(const QString& sha, QString* errorMsg = nullptr);

	/* Reads one file at a revision; an empty QByteArray and ok=false on failure. */
	QByteArray showFile(const QString& sha, const QString& path,
						bool* ok = nullptr) const;

	/* Make a tag at the given commit (or HEAD if sha empty). */
	bool tag(const QString& name, const QString& sha = {},
			 QString* errorMsg = nullptr);

	/* Deletes the HEAD commit by hard-resetting to its parent. Older commits are immutable to keep
	 * history coherent; false otherwise. */
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
