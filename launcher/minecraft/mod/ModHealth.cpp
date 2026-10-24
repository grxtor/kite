/* SPDX-FileCopyrightText: 2026 grxtor
 * SPDX-FileContributor: grxtor
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (C) 2026 grxtor
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "ModHealth.h"

#include <QSet>
#include <algorithm>

#include "ModHealthIndex.h"
#include "ModHealthLoaders.h"
#include "VersionRange.h"

namespace ModHealth
{
	namespace
	{
		using VersionRange::Match;
		using namespace detail;

		bool isBlocking(Kind kind)
		{
			return kind != Kind::Discouraged && kind != Kind::WrongLoader;
		}

		bool reportedBefore(const Issue& a, const Issue& b)
		{
			if (a.blocking != b.blocking)
				return a.blocking;
			if (const int byName =
					QString::compare(a.name, b.name, Qt::CaseInsensitive);
				byName != 0) {
				return byName < 0;
			}
			return a.key < b.key;
		}

		class Checker
		{
		  public:
			Checker(const QList<Entry>& entries,
					const Environment& environment);

			QList<Issue> run();

		  private:
			void checkEntry(int entry);
			void checkRequired(int entry, const ModDependency& dependency,
							   bool folded);
			void checkClash(int entry, const ModDependency& dependency,
							bool folded);
			void checkDuplicates();

			Issue issueFor(Kind kind, int entry) const;
			Issue issueFor(Kind kind, int entry,
						   const ModDependency& dependency) const;
			void pointAt(Issue& issue, int other, const QString& version) const;
			void report(const Issue& issue);

			const FolderIndex m_index;
			const QStringList m_loaders;
			const QString m_minecraft;
			QSet<QString> m_reported;
			QList<Issue> m_issues;
		};

		Checker::Checker(const QList<Entry>& entries,
						 const Environment& environment)
			: m_index(entries, environment),
			  m_loaders(lowered(environment.loaders)),
			  m_minecraft(environment.minecraft)
		{
		}

		Issue Checker::issueFor(Kind kind, int entry) const
		{
			Issue issue;
			issue.kind = kind;
			issue.blocking = isBlocking(kind);
			issue.key = m_index.entries.at(entry).key;
			issue.name = m_index.entries.at(entry).name;
			return issue;
		}

		Issue Checker::issueFor(Kind kind, int entry,
								const ModDependency& dependency) const
		{
			Issue issue = issueFor(kind, entry);
			issue.otherId = dependency.id;
			issue.otherName = dependency.id;
			issue.dependency = dependency;
			return issue;
		}

		void Checker::pointAt(Issue& issue, int other,
							  const QString& version) const
		{
			issue.otherKey = m_index.entries.at(other).key;
			issue.otherName = m_index.entries.at(other).name;
			issue.foundVersion = version;
		}

		void Checker::report(const Issue& issue)
		{
			const QString identity =
				QStringLiteral("%1\n%2\n%3")
					.arg(int(issue.kind))
					.arg(issue.key, issue.otherId.toLower());
			if (m_reported.contains(identity))
				return;
			m_reported.insert(identity);
			m_issues.append(issue);
		}

		void Checker::checkRequired(int entry, const ModDependency& dependency,
									bool folded)
		{
			const QString id = dependency.id.toLower();
			if (id.isEmpty() || RuntimeIds.contains(id))
				return;

			if (m_index.builtIns.contains(id)) {
				const QString version = m_index.builtIns.value(id);
				if (VersionRange::matches(version, dependency) != Match::No)
					return;
				for (const QString& extra :
					 alsoAnswersTo(id, m_loaders, m_minecraft)) {
					if (VersionRange::matches(extra, dependency) != Match::No)
						return;
				}
				Issue issue = issueFor(id == MinecraftId
										   ? Kind::WrongMinecraft
										   : Kind::WrongLoaderVersion,
									   entry, dependency);
				issue.foundVersion = version;
				report(issue);
				return;
			}
			if (LoaderIds.contains(id))
				return;

			const QList<Provider> enabled = m_index.enabledFor(id, folded);
			if (!enabled.isEmpty()) {
				for (const Provider& provider : enabled) {
					if (VersionRange::matches(provider.version, dependency) !=
						Match::No) {
						return;
					}
				}
				Issue issue =
					issueFor(Kind::WrongDependencyVersion, entry, dependency);
				pointAt(issue, enabled.first().entry, enabled.first().version);
				report(issue);
				return;
			}

			const QList<Provider> disabled = m_index.disabledFor(id, folded);
			if (!disabled.isEmpty()) {
				Issue issue =
					issueFor(Kind::DisabledDependency, entry, dependency);
				pointAt(issue, disabled.first().entry, QString());
				report(issue);
				return;
			}
			report(issueFor(Kind::MissingDependency, entry, dependency));
		}

		void Checker::checkClash(int entry, const ModDependency& dependency,
								 bool folded)
		{
			const QString id = dependency.id.toLower();
			if (id.isEmpty())
				return;
			const bool breaks =
				dependency.kind == ModDependency::Kind::Incompatible;
			const Kind kind = breaks ? Kind::Incompatible : Kind::Discouraged;

			if (m_index.builtIns.contains(id) &&
				VersionRange::matches(m_index.builtIns.value(id), dependency) ==
					Match::Yes) {
				report(issueFor(kind, entry, dependency));
				return;
			}
			for (const Provider& provider : m_index.enabledFor(id, folded)) {
				// Never itself, and not a file the loader skips anyway.
				if (provider.entry == entry ||
					!m_index.isLoaded(provider.entry)) {
					continue;
				}
				if (VersionRange::matches(provider.version, dependency) ==
					Match::Yes) {
					Issue issue = issueFor(kind, entry, dependency);
					pointAt(issue, provider.entry, QString());
					report(issue);
					return;
				}
			}
		}

		void Checker::checkEntry(int entry)
		{
			const Entry& file = m_index.entries.at(entry);
			if (!file.enabled)
				return;
			if (m_index.isWrongLoader(entry)) {
				report(issueFor(Kind::WrongLoader, entry));
				return;
			}

			// A jar built for several loaders lists its needs once for each;
			// only the list of the loader that reads it counts.
			const QString loader = m_index.readingLoader(file);
			if (loader.isEmpty())
				return;
			// Connector, not Fabric, reads this file: its ids are Forge's.
			const bool folded = m_index.readsThroughConnector(loader);
			for (const ModDependency& dependency : file.details.dependencies) {
				if (dependency.loader.toLower() != loader)
					continue;
				if (dependency.kind == ModDependency::Kind::Required)
					checkRequired(entry, dependency, folded);
				else
					checkClash(entry, dependency, folded);
			}
		}

		void Checker::checkDuplicates()
		{
			const QList<Entry>& files = m_index.entries;
			QHash<QString, QList<int>> copiesOf;
			for (int i = 0; i < files.size(); ++i) {
				const QString id = files.at(i).details.mod_id.toLower();
				if (!id.isEmpty() && m_index.isLoaded(i))
					copiesOf[id].append(i);
			}

			for (int i = 0; i < files.size(); ++i) {
				const QString id = files.at(i).details.mod_id;
				const QList<int> copies = copiesOf.value(id.toLower());
				if (id.isEmpty() || copies.size() < 2 || !copies.contains(i))
					continue;

				// Each points at a copy other than itself.
				const int other = copies.first() == i ? copies.at(1)
													   : copies.first();
				Issue issue = issueFor(Kind::Duplicate, i);
				issue.otherId = id;
				pointAt(issue, other, QString());
				report(issue);
			}
		}

		QList<Issue> Checker::run()
		{
			for (int i = 0; i < m_index.entries.size(); ++i)
				checkEntry(i);
			checkDuplicates();
			std::stable_sort(m_issues.begin(), m_issues.end(), reportedBefore);
			return m_issues;
		}
	} // namespace

	QList<Issue> check(const QList<Entry>& entries,
					   const Environment& environment)
	{
		if (environment.loaders.isEmpty())
			return {};
		return Checker(entries, environment).run();
	}
} // namespace ModHealth
