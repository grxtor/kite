/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-License-Identifier: Apache-2.0
 *
 * LearningStore: persisted (rule, instance, signature) -> outcome table, a JSON file under
 * <plugin_data>/learn.json written atomically. The plugin uses it to:
 *
 *   1) score known rule hits so the most useful ones float to the top of the suggestions;
 *
 *   2) detect novel error fingerprints: a stable signature of each crash report (the top-of-stack class
 *      names, lower-cased, mod-version suffixes stripped) is remembered in a separate "novel" table, and
 *      when it recurs the user may promote it to a user rule with a custom title and advice. That is the
 *      "learning": new rules from real crashes, no LLM.
 */

#pragma once

#include "plugin/sdk/mmco_cxx_sdk.hpp"

struct OutcomeStats {
	int helped = 0;
	int didNotHelp = 0;
	int seen = 0;
	qint64 lastSeenSecs = 0;
};

struct NovelFingerprint {
	QString signature;
	QString sampleLine; // the line that produced the signature
	QString sampleInstance;
	int occurrences = 0;
	qint64 firstSeenSecs = 0;
	qint64 lastSeenSecs = 0;
};

class LearningStore
{
  public:
	bool open(const QString& filePath);
	bool save();

	double scoreFor(const QString& ruleId, const QString& instanceId) const;

	void recordSeen(const QString& ruleId, const QString& instanceId);
	void recordHelped(const QString& ruleId, const QString& instanceId);
	void recordDidNotHelp(const QString& ruleId, const QString& instanceId);

	/* Computes a deterministic crash signature from log text; stable across launches and portable
	 * between machines, so useful for shared crash reports. */
	static QString fingerprint(const QString& logText);

	void recordNovel(const QString& signature, const QString& sampleLine,
					 const QString& instanceId);
	QList<NovelFingerprint> novelFingerprints() const;

	/* Forgets a signature, typically after the user promotes it to a rule. */
	void forgetNovel(const QString& signature);

  private:
	struct Key {
		QString ruleId;
		QString instanceId;
		bool operator==(const Key& o) const
		{
			return ruleId == o.ruleId && instanceId == o.instanceId;
		}
	};
	friend uint qHash(const LearningStore::Key&, uint seed) noexcept;

	QHash<Key, OutcomeStats> m_outcomes;
	QHash<QString, NovelFingerprint> m_novel;
	QString m_path;
};

uint qHash(const LearningStore::Key& k, uint seed = 0) noexcept;
