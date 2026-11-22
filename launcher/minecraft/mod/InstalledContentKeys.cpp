/* SPDX-FileCopyrightText: 2026 grxtor
 * SPDX-FileContributor: grxtor
 * SPDX-License-Identifier: Apache-2.0
 */

#include "InstalledContentKeys.h"

#include <QRegularExpression>
#include <QStringList>

#include "Mod.h"
#include "ModMetadataIndex.h"

namespace
{
	const QStringList& loaderWords()
	{
		static const QStringList words = {
			QStringLiteral("fabric"),	QStringLiteral("forge"),
			QStringLiteral("neoforge"), QStringLiteral("quilt"),
			QStringLiteral("mc"),		QStringLiteral("minecraft"),
		};
		return words;
	}

	/* "0.6.5", "v2", "mc1.21", "1.20.1", "r5.3": where a file name's version
	 * part starts. */
	bool looksLikeVersion(const QString& token)
	{
		static const QRegularExpression version(
			QStringLiteral("^(?:v|mc|r)?\\d"),
			QRegularExpression::CaseInsensitiveOption);
		return version.match(token).hasMatch();
	}

	/* The name tokens of a file name, before its version. */
	QStringList nameTokens(const QString& fileName)
	{
		QString stem = ModMetadataIndex::canonicalFileName(fileName);
		static const QRegularExpression extension(
			QStringLiteral("\\.(?:jar|zip|litemod)$"),
			QRegularExpression::CaseInsensitiveOption);
		stem.remove(extension);

		static const QRegularExpression separators(
			QStringLiteral("[\\s\\-_+\\[\\]()]+"));
		const QStringList tokens =
			stem.split(separators, Qt::SkipEmptyParts);

		QStringList name;
		for (const QString& token : tokens) {
			if (looksLikeVersion(token)) {
				/* A version in front ("[1.20.1] Name") is skipped; one after
				 * the name ends it. */
				if (name.isEmpty()) {
					continue;
				}
				break;
			}
			name.append(token.toLower());
		}
		return name;
	}

	void add(QSet<QString>& keys, const QString& candidate)
	{
		if (candidate.size() > 1) {
			keys.insert(candidate);
		}
	}
} // namespace

QString InstalledContentKeys::key(const QString& text)
{
	QString out;
	out.reserve(text.size());
	for (const QChar c : text.toLower()) {
		if (c.isLetterOrNumber()) {
			out.append(c);
		}
	}
	return out;
}

QString InstalledContentKeys::fileKey(const QString& fileName)
{
	return key(nameTokens(fileName).join(QString()));
}

QSet<QString> InstalledContentKeys::keysOf(const QList<File>& files)
{
	QSet<QString> keys;
	for (const File& file : files) {
		add(keys, key(file.modId));
		/* normalizeName() takes a loader marker off first: "Sodium (Fabric)". */
		add(keys, key(ModMetadataIndex::normalizeName(file.name)));

		QStringList tokens = nameTokens(file.fileName);
		add(keys, key(tokens.join(QString())));
		while (tokens.size() > 1 && loaderWords().contains(tokens.last())) {
			tokens.removeLast();
			add(keys, key(tokens.join(QString())));
		}
	}
	return keys;
}

QSet<QString> InstalledContentKeys::keysOf(const QList<Mod>& mods)
{
	QList<File> files;
	files.reserve(mods.size());
	for (const Mod& mod : mods) {
		File file;
		file.fileName = mod.filename().fileName();
		const ModDetails& details = mod.details();
		file.modId = details.mod_id;
		file.name = details.name;
		files.append(file);
	}
	return keysOf(files);
}

bool InstalledContentKeys::contains(const QSet<QString>& keys,
									const QString& slug, const QString& name)
{
	if (keys.isEmpty()) {
		return false;
	}
	const QString slugKey = key(slug);
	if (slugKey.size() > 1 && keys.contains(slugKey)) {
		return true;
	}
	const QString nameKey = key(ModMetadataIndex::normalizeName(name));
	return nameKey.size() > 1 && keys.contains(nameKey);
}
