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

#include "ImportCopy.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSet>

#include "SafeFiles.h"

namespace LauncherImport
{
	namespace
	{
		/* Deeper than any game folder nests; a deeper tree is cut there. */
		constexpr int kMaxDepth = 64;
		/* Files and folders one copy may list. Links inside the folder can
		 * make one tree appear many times over; past this the copy is
		 * refused instead of listed for ever. */
		constexpr int kMaxEntries = 2000000;
		constexpr qint64 kCopyChunkBytes = 1024 * 1024;

		struct Walker {
			QString root;
			const std::atomic_bool* cancel = nullptr;
			/* The real paths of the folders being walked, outermost first: a
			 * link back to one of them is a loop. */
			QSet<QString> ancestors;
			int entries = 0;
			CopyPlan plan;

			bool stopped() const
			{
				return cancel && cancel->load();
			}

			void visit(const QString& path, const QString& relative, int depth)
			{
				if (stopped()) {
					plan.cancelled = true;
					return;
				}
				if (++entries > kMaxEntries) {
					plan.tooLarge = true;
					return;
				}
				QFileInfo info(path);
				if (isLink(info)) {
					if (!resolvesInside(root, path)) {
						++plan.skippedLinks;
						return;
					}
					info = QFileInfo(info.canonicalFilePath());
				}
				if (info.isDir()) {
					/* By its real path, so a link back up the tree is seen. */
					const QString real = info.canonicalFilePath();
					if (ancestors.contains(real)) {
						return;
					}
					plan.folders.append(relative);
					if (depth >= kMaxDepth) {
						return;
					}
					ancestors.insert(real);
					const QFileInfoList children = QDir(info.absoluteFilePath())
						.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot |
										   QDir::Hidden | QDir::System,
									   QDir::Name);
					for (const QFileInfo& entry : children) {
						visit(entry.absoluteFilePath(),
							  relative + QLatin1Char('/') + entry.fileName(),
							  depth + 1);
					}
					ancestors.remove(real);
				} else if (info.isFile()) {
					plan.files.append({info.absoluteFilePath(), relative, info.size()});
					plan.bytes += info.size();
				}
			}
		};

		bool copyOne(const CopyPlan::File& file, const QString& target,
					 const std::atomic_bool& cancel,
					 std::atomic<qint64>* copied, QString* error)
		{
			QFile in(file.source);
			if (!in.open(QIODevice::ReadOnly)) {
				*error = QCoreApplication::translate(
							 "LauncherImport", "Could not read %1.")
							 .arg(file.relative);
				return false;
			}
			QFile out(target);
			if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
				*error = QCoreApplication::translate(
							 "LauncherImport", "Could not write %1.")
							 .arg(file.relative);
				return false;
			}
			while (!in.atEnd()) {
				if (cancel.load()) {
					return false;
				}
				const QByteArray chunk = in.read(kCopyChunkBytes);
				if (chunk.isEmpty() && in.error() != QFileDevice::NoError) {
					*error = QCoreApplication::translate(
								 "LauncherImport", "Could not read %1.")
								 .arg(file.relative);
					return false;
				}
				if (out.write(chunk) != chunk.size()) {
					*error = QCoreApplication::translate(
								 "LauncherImport", "Could not write %1.")
								 .arg(file.relative);
					return false;
				}
				if (copied) {
					copied->fetch_add(chunk.size());
				}
			}
			/* Worlds and packs sort by date in the game; keep theirs. */
			out.flush();
			out.setFileTime(in.fileTime(QFileDevice::FileModificationTime),
							QFileDevice::FileModificationTime);
			return true;
		}
	} // namespace

	QStringList copiedFolders()
	{
		/* defaultconfigs, kubejs and scripts are part of a modpack: without
		 * them its recipes and settings are not the pack's. */
		return {QStringLiteral("saves"),		  QStringLiteral("mods"),
				QStringLiteral("config"),		  QStringLiteral("defaultconfigs"),
				QStringLiteral("resourcepacks"),  QStringLiteral("texturepacks"),
				QStringLiteral("shaderpacks"),	  QStringLiteral("kubejs"),
				QStringLiteral("scripts")};
	}

	QStringList copiedFiles()
	{
		/* optionsof.txt and optionsshaders.txt are OptiFine's and Iris's
		 * share of the options. */
		return {QStringLiteral("options.txt"), QStringLiteral("optionsof.txt"),
				QStringLiteral("optionsshaders.txt"),
				QStringLiteral("servers.dat")};
	}

	CopyPlan planCopy(const QString& gameDir, const std::atomic_bool* cancel)
	{
		Walker walker;
		walker.cancel = cancel;
		walker.root = QFileInfo(gameDir).canonicalFilePath();
		if (walker.root.isEmpty()) {
			return walker.plan;
		}
		for (const QString& name : copiedFolders()) {
			const QString path = walker.root + QLatin1Char('/') + name;
			if (QFileInfo(path).isDir()) {
				walker.visit(path, name, 1);
			} else if (isLink(QFileInfo(path))) {
				/* A dangling link, or one to a file where a folder belongs. */
				++walker.plan.skippedLinks;
			}
		}
		for (const QString& name : copiedFiles()) {
			const QString path = walker.root + QLatin1Char('/') + name;
			const QFileInfo info(path);
			if (info.isFile() || isLink(info)) {
				walker.visit(path, name, 1);
			}
		}
		return walker.plan;
	}

	bool runCopy(const CopyPlan& plan, const QString& destination,
				 const std::atomic_bool& cancel, std::atomic<qint64>* copied,
				 QString* error)
	{
		QDir dest(destination);
		if (!dest.mkpath(QStringLiteral("."))) {
			*error = QCoreApplication::translate(
				"LauncherImport", "Could not make the instance folder.");
			return false;
		}
		for (const QString& folder : plan.folders) {
			if (!dest.mkpath(folder)) {
				*error = QCoreApplication::translate(
							 "LauncherImport", "Could not write %1.")
							 .arg(folder);
				return false;
			}
		}
		for (const CopyPlan::File& file : plan.files) {
			if (cancel.load()) {
				return false;
			}
			if (!copyOne(file, dest.filePath(file.relative), cancel, copied,
						 error)) {
				return false;
			}
		}
		return !cancel.load();
	}
} // namespace LauncherImport
