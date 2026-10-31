/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-License-Identifier: Apache-2.0
 *
 * RuleEngine: loads JSON rule packs and matches them against log text. A rule has:
 *
 *   - id        unique stable identifier (used by LearningStore)
 *   - title     short human-readable description of the symptom
 *   - severity  "low" / "medium" / "high"
 *   - patterns  list of regex strings (any match counts)
 *   - advice    Markdown remediation text shown in the UI
 *   - tags      bag of strings used for grouping
 *
 * After an analysis the caller may register that a rule's advice solved the crash or did not; the
 * LearningStore then bumps a per-(rule, instance) score that boosts the rule's suggested order for
 * that instance.
 */

#pragma once

#include "plugin/sdk/mmco_cxx_sdk.hpp"
#include <QRegularExpression>

enum class Severity { Low, Medium, High };

struct Rule {
	QString id;
	QString title;
	Severity severity = Severity::Medium;
	QStringList patternStrings;
	QList<QRegularExpression> patterns;
	QString advice;
	QStringList tags;
	QString packName;	// source rule pack
	QJsonArray actions; // free-form action descriptors from the JSON
};

struct Match {
	QString ruleId;
	QString ruleTitle;
	Severity severity = Severity::Medium;
	QString advice;
	QString matchedLine;
	int line = -1;
	double score = 0.0; // populated from LearningStore
};

class RuleEngine
{
  public:
	bool loadDirectory(const QString& dir, QString* errorMsg = nullptr);

	/* Loads a rule pack from raw JSON bytes (for user-supplied packs that are not on disk). */
	bool loadFromBytes(const QByteArray& bytes, const QString& packName,
					   QString* errorMsg = nullptr);

	const QList<Rule>& rules() const
	{
		return m_rules;
	}
	int ruleCount() const
	{
		return m_rules.size();
	}

	/* Run every rule against `text` and return the matches in
	 * descending severity, then descending learning score. */
	QList<Match> analyse(const QString& text) const;

	int findRuleIndex(const QString& id) const;

  private:
	QList<Rule> m_rules;
};
