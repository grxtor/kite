/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileContributor: Project Tick
 * SPDX-FileContributor: grxtor
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (C) 2026 Project Tick
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

#include "plugin/PluginSignature.h"
#include "plugin/MMCOFormat.h"
#include "Logging.h"
#include <QByteArray>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutex>
#include <QMutexLocker>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QtEndian>
#include <memory>

/*
 * KITE_PLUGIN_SIGNATURES is defined in launcher/CMakeLists.txt:
 *   1 → full GpgME-backed verifier (Linux, macOS, MinGW Windows)
 *   0 → stub verifier that always reports modules as unsigned. Used on
 *       MSVC where upstream gpgme has no working build (autotools-only,
 *       C++ ABI incompatibilities with MSVC).
 */
#ifndef KITE_PLUGIN_SIGNATURES
#define KITE_PLUGIN_SIGNATURES 1
#endif

#if KITE_PLUGIN_SIGNATURES
#include <gpgme++/context.h>
#include <gpgme++/data.h>
#include <gpgme++/global.h>
#include <gpgme++/key.h>
#include <gpgme++/verificationresult.h>
#include <gpgme++/engineinfo.h>
#endif

namespace PluginSignature
{

	namespace
	{
		/* Mutex protects the static keyring-path override, the cached
		 * verification results, and the long-lived GpgME context. We use
		 * a single recursive-style guard for everything because the
		 * critical sections are tiny — none of them perform I/O while
		 * holding the lock. */
		QMutex g_mutex;
		QString g_keyringPath;

#if KITE_PLUGIN_SIGNATURES
		bool g_gpgmeInitialised = false;

		void ensureGpgmeInit()
		{
			if (g_gpgmeInitialised)
				return;
			GpgME::initializeLibrary();
			g_gpgmeInitialised = true;
		}

		/* ─── Persistent GpgME context ────────────────────────────
		 *
		 * Creating a fresh GpgME::Context for every plugin is the
		 * single biggest bottleneck in the original implementation —
		 * each construction:
		 *   • forks/execs gpg-agent (or pipes to an existing one),
		 *   • loads the trusted keyring from disk,
		 *   • parses every key in it,
		 *   • does a TLS-style handshake over the agent socket.
		 *
		 * On a typical Linux desktop with 8 plugins that's 8 × ~80 ms
		 * = ~640 ms of pure startup latency. Keeping a single context
		 * alive for the life of the process drops that to about
		 * 8 × 5 ms (= one cheap RPC per verify).
		 *
		 * The context is created lazily on first verification and
		 * torn down implicitly at process exit (std::unique_ptr in
		 * static storage). Guarded by g_mutex; GpgME contexts are not
		 * thread-safe individually, but our verify path serialises
		 * around g_mutex anyway, so the singleton is safe. */
		std::unique_ptr<GpgME::Context> g_gpgCtx;
		QString g_gpgCtxHome; // tracks the home dir the singleton was built for

		GpgME::Context* gpgContextLocked()
		{
			// Always called with g_mutex held.
			if (g_gpgCtx && g_gpgCtxHome == g_keyringPath)
				return g_gpgCtx.get();

			// Either we don't have one yet, or the keyring path changed
			// since the last call — rebuild.
			g_gpgCtx.reset(GpgME::Context::createForProtocol(GpgME::OpenPGP));
			if (!g_gpgCtx)
				return nullptr;

			if (!g_keyringPath.isEmpty()) {
				const QByteArray homeBytes = g_keyringPath.toLocal8Bit();
				g_gpgCtx->setEngineHomeDirectory(homeBytes.constData());
			}
			g_gpgCtxHome = g_keyringPath;
			return g_gpgCtx.get();
		}
#endif

		/* ─── Persistent verification cache ────────────────────────
		 *
		 * Keyed on (absolute path, file size, mtime in milliseconds).
		 * The (size, mtime) tuple is the canonical "did anything
		 * change?" fingerprint on Linux/macOS/Windows. If a packager
		 * touches the file (chmod, chown, attribute change) without
		 * actually rewriting bytes mtime stays put, but size doesn't
		 * change either, so the cache stays valid. If they *do*
		 * rewrite bytes (strip, chrpath, repackage) mtime moves and
		 * the cache entry is invalidated automatically.
		 *
		 * The schema is intentionally simple JSON so it's easy to
		 * inspect and easy to delete by hand if something goes wrong:
		 *
		 *   {
		 *     "schema": 1,
		 *     "entries": [
		 *       {
		 *         "path": "/usr/lib/mmcmodules/BackupSystem.mmco",
		 *         "size": 524288,
		 *         "mtime_ms": 1715608800123,
		 *         "state": "Valid",
		 *         "detail": "Good signature",
		 *         "fingerprint": "0123ABCD..."
		 *       },
		 *       …
		 *     ]
		 *   }
		 *
		 * Cache size is bounded by the number of installed plugins
		 * (~tens), so we don't bother with LRU eviction.
		 */
		struct CacheKey {
			QString path;
			qint64 size;
			qint64 mtimeMs;
			bool operator==(const CacheKey& o) const
			{
				return size == o.size && mtimeMs == o.mtimeMs && path == o.path;
			}
		};
		uint qHash(const CacheKey& k, uint seed = 0) noexcept
		{
			return ::qHash(k.path, seed) ^ ::qHash(k.size, seed) ^
				   ::qHash(k.mtimeMs, seed);
		}

		struct CacheEntry {
			PluginSignatureState state = PluginSignatureState::NotChecked;
			QString detail;
			QString fingerprint;
		};

		QHash<CacheKey, CacheEntry> g_cache;
		QString g_cachePath;
		bool g_cacheDirty = false;

		const char* stateToToken(PluginSignatureState s)
		{
			switch (s) {
				case PluginSignatureState::Valid:
					return "Valid";
				case PluginSignatureState::Untrusted:
					return "Untrusted";
				case PluginSignatureState::BadSignature:
					return "BadSignature";
				case PluginSignatureState::Malformed:
					return "Malformed";
				case PluginSignatureState::Absent:
					return "Absent";
				case PluginSignatureState::Error:
					return "Error";
				case PluginSignatureState::NotChecked:
					return "NotChecked";
			}
			return "NotChecked";
		}

		PluginSignatureState tokenToState(const QString& t)
		{
			if (t == QLatin1String("Valid"))
				return PluginSignatureState::Valid;
			if (t == QLatin1String("Untrusted"))
				return PluginSignatureState::Untrusted;
			if (t == QLatin1String("BadSignature"))
				return PluginSignatureState::BadSignature;
			if (t == QLatin1String("Malformed"))
				return PluginSignatureState::Malformed;
			if (t == QLatin1String("Absent"))
				return PluginSignatureState::Absent;
			if (t == QLatin1String("Error"))
				return PluginSignatureState::Error;
			return PluginSignatureState::NotChecked;
		}

		void loadCacheLocked()
		{
			g_cache.clear();
			if (g_cachePath.isEmpty())
				return;
			QFile f(g_cachePath);
			if (!f.open(QIODevice::ReadOnly))
				return;
			QJsonParseError jerr;
			auto doc = QJsonDocument::fromJson(f.readAll(), &jerr);
			if (jerr.error != QJsonParseError::NoError || !doc.isObject())
				return;
			auto root = doc.object();
			if (root.value("schema").toInt(0) != 1)
				return;
			for (auto v : root.value("entries").toArray()) {
				auto obj = v.toObject();
				CacheKey k;
				k.path = obj.value("path").toString();
				k.size = qint64(obj.value("size").toDouble(0));
				k.mtimeMs = qint64(obj.value("mtime_ms").toDouble(0));
				if (k.path.isEmpty())
					continue;
				CacheEntry e;
				e.state = tokenToState(obj.value("state").toString());
				e.detail = obj.value("detail").toString();
				e.fingerprint = obj.value("fingerprint").toString();
				g_cache.insert(k, e);
			}
		}

		void saveCacheLocked()
		{
			if (g_cachePath.isEmpty() || !g_cacheDirty)
				return;
			QDir().mkpath(QFileInfo(g_cachePath).absolutePath());

			QJsonObject root;
			root["schema"] = 1;
			QJsonArray entries;
			for (auto it = g_cache.constBegin(); it != g_cache.constEnd();
				 ++it) {
				QJsonObject obj;
				obj["path"] = it.key().path;
				obj["size"] = double(it.key().size);
				obj["mtime_ms"] = double(it.key().mtimeMs);
				obj["state"] = QLatin1String(stateToToken(it.value().state));
				obj["detail"] = it.value().detail;
				obj["fingerprint"] = it.value().fingerprint;
				entries.append(obj);
			}
			root["entries"] = entries;

			QSaveFile out(g_cachePath);
			if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate))
				return;
			out.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
			out.commit();
			g_cacheDirty = false;
		}

		/* OSI-approved SPDX identifier allow-list.
		 *
		 * Source of truth: the OSI License Index
		 *   <https://opensource.org/licenses/>
		 * cross-referenced against the SPDX License List
		 *   <https://spdx.org/licenses/>
		 *
		 * Snapshot taken from SPDX License List 3.24+ — every identifier
		 * in the "OSI Approved" section below is flagged
		 * "OSI Approved? Yes" upstream. A handful of non-OSI but widely
		 * accepted FSF-libre / public-domain identifiers (CC0-1.0,
		 * Unlicense, WTFPL, Vim, BSD-3-Clause-Clear, …) follow in their
		 * own block so that public-domain dedications and niche
		 * permissive picks do not force authors into the signing pipeline
		 * unnecessarily.
		 *
		 * Names are stored lower-case so the lookup is case-insensitive.
		 *
		 * Maintenance: when SPDX adds a new OSI-approved license, append
		 * the lower-cased identifier here. Do NOT remove identifiers —
		 * doing so retroactively breaks every plugin that uses them. */
		const QSet<QString>& ossSpdxIds()
		{
			static const QSet<QString> ids = {
				/* ══ OSI-approved ══════════════════════════════════ */

				/* ── 0–9 ─────────────────────────────────────────── */
				QStringLiteral("0bsd"),

				/* ── A ───────────────────────────────────────────── */
				QStringLiteral("aal"),
				QStringLiteral("afl-1.1"),
				QStringLiteral("afl-1.2"),
				QStringLiteral("afl-2.0"),
				QStringLiteral("afl-2.1"),
				QStringLiteral("afl-3.0"),
				QStringLiteral("agpl-3.0"),
				QStringLiteral("agpl-3.0-only"),
				QStringLiteral("agpl-3.0-or-later"),
				QStringLiteral("apache-1.1"),
				QStringLiteral("apache-2.0"),
				QStringLiteral("apl-1.0"),
				QStringLiteral("apsl-1.0"),
				QStringLiteral("apsl-1.1"),
				QStringLiteral("apsl-1.2"),
				QStringLiteral("apsl-2.0"),
				QStringLiteral("artistic-1.0"),
				QStringLiteral("artistic-1.0-cl8"),
				QStringLiteral("artistic-1.0-perl"),
				QStringLiteral("artistic-2.0"),

				/* ── B ───────────────────────────────────────────── */
				QStringLiteral("bsd-1-clause"),
				QStringLiteral("bsd-2-clause"),
				QStringLiteral("bsd-2-clause-patent"),
				QStringLiteral("bsd-3-clause"),
				QStringLiteral("bsd-3-clause-lbnl"),
				QStringLiteral("bsd-3-clause-modification"),
				QStringLiteral("bsl-1.0"),

				/* ── C ───────────────────────────────────────────── */
				QStringLiteral("cal-1.0"),
				QStringLiteral("cal-1.0-combined-work-exception"),
				QStringLiteral("catosl-1.1"),
				QStringLiteral("cddl-1.0"),
				QStringLiteral("cddl-1.1"),
				QStringLiteral("cecill-2.1"),
				QStringLiteral("cern-ohl-p-2.0"),
				QStringLiteral("cern-ohl-s-2.0"),
				QStringLiteral("cern-ohl-w-2.0"),
				QStringLiteral("cnri-python"),
				QStringLiteral("cpal-1.0"),
				QStringLiteral("cpl-1.0"),
				QStringLiteral("cua-opl-1.0"),

				/* ── E ───────────────────────────────────────────── */
				QStringLiteral("ecl-1.0"),
				QStringLiteral("ecl-2.0"),
				QStringLiteral("efl-1.0"),
				QStringLiteral("efl-2.0"),
				QStringLiteral("entessa"),
				QStringLiteral("epl-1.0"),
				QStringLiteral("epl-2.0"),
				QStringLiteral("eudatagrid"),
				QStringLiteral("eupl-1.1"),
				QStringLiteral("eupl-1.2"),

				/* ── F ───────────────────────────────────────────── */
				QStringLiteral("fair"),
				QStringLiteral("frameworx-1.0"),

				/* ── G ───────────────────────────────────────────── */
				QStringLiteral("gpl-2.0"),
				QStringLiteral("gpl-2.0-only"),
				QStringLiteral("gpl-2.0-or-later"),
				QStringLiteral("gpl-3.0"),
				QStringLiteral("gpl-3.0-only"),
				QStringLiteral("gpl-3.0-or-later"),

				/* ── H ───────────────────────────────────────────── */
				QStringLiteral("hpnd"),

				/* ── I ───────────────────────────────────────────── */
				QStringLiteral("intel"),
				QStringLiteral("ipa"),
				QStringLiteral("ipl-1.0"),
				QStringLiteral("isc"),

				/* ── J ───────────────────────────────────────────── */
				QStringLiteral("jam"),

				/* ── L ───────────────────────────────────────────── */
				QStringLiteral("lgpl-2.0"),
				QStringLiteral("lgpl-2.0-only"),
				QStringLiteral("lgpl-2.0-or-later"),
				QStringLiteral("lgpl-2.1"),
				QStringLiteral("lgpl-2.1-only"),
				QStringLiteral("lgpl-2.1-or-later"),
				QStringLiteral("lgpl-3.0"),
				QStringLiteral("lgpl-3.0-only"),
				QStringLiteral("lgpl-3.0-or-later"),
				QStringLiteral("liliq-p-1.1"),
				QStringLiteral("liliq-r-1.1"),
				QStringLiteral("liliq-rplus-1.1"),
				QStringLiteral("lppl-1.3c"),

				/* ── M ───────────────────────────────────────────── */
				QStringLiteral("miros"),
				QStringLiteral("mit"),
				QStringLiteral("mit-0"),
				QStringLiteral("mit-modern-variant"),
				QStringLiteral("motosoto"),
				QStringLiteral("mpl-1.0"),
				QStringLiteral("mpl-1.1"),
				QStringLiteral("mpl-2.0"),
				QStringLiteral("mpl-2.0-no-copyleft-exception"),
				QStringLiteral("ms-pl"),
				QStringLiteral("ms-rl"),
				QStringLiteral("mulanpsl-2.0"),
				QStringLiteral("multics"),

				/* ── N ───────────────────────────────────────────── */
				QStringLiteral("nasa-1.3"),
				QStringLiteral("naumen"),
				QStringLiteral("ncsa"),
				QStringLiteral("nokia"),
				QStringLiteral("nposl-3.0"),
				QStringLiteral("ntp"),

				/* ── O ───────────────────────────────────────────── */
				QStringLiteral("ofl-1.1"),
				QStringLiteral("ofl-1.1-no-rfn"),
				QStringLiteral("ofl-1.1-rfn"),
				QStringLiteral("oldap-2.8"),
				QStringLiteral("oset-pl-2.1"),
				QStringLiteral("osl-1.0"),
				QStringLiteral("osl-2.0"),
				QStringLiteral("osl-2.1"),
				QStringLiteral("osl-3.0"),

				/* ── P ───────────────────────────────────────────── */
				QStringLiteral("php-3.0"),
				QStringLiteral("php-3.01"),
				QStringLiteral("postgresql"),
				QStringLiteral("python-2.0"),
				QStringLiteral("python-2.0.1"),

				/* ── Q ───────────────────────────────────────────── */
				QStringLiteral("qpl-1.0"),

				/* ── R ───────────────────────────────────────────── */
				QStringLiteral("rpl-1.1"),
				QStringLiteral("rpl-1.5"),
				QStringLiteral("rpsl-1.0"),
				QStringLiteral("rscpl"),

				/* ── S ───────────────────────────────────────────── */
				QStringLiteral("simpl-2.0"),
				QStringLiteral("sissl"),
				QStringLiteral("sleepycat"),
				QStringLiteral("spl-1.0"),

				/* ── U ───────────────────────────────────────────── */
				QStringLiteral("ucl-1.0"),
				QStringLiteral("upl-1.0"),

				/* ── V ───────────────────────────────────────────── */
				QStringLiteral("vsl-1.0"),

				/* ── W ───────────────────────────────────────────── */
				QStringLiteral("w3c"),
				QStringLiteral("w3c-19980720"),
				QStringLiteral("w3c-20150513"),
				QStringLiteral("watcom-1.0"),

				/* ── X ───────────────────────────────────────────── */
				QStringLiteral("xnet"),

				/* ── Z ───────────────────────────────────────────── */
				QStringLiteral("zlib"),
				QStringLiteral("zpl-2.0"),
				QStringLiteral("zpl-2.1"),

				/* ══ Non-OSI but widely accepted libre ════════════ *
				 * These pass FSF / Debian / Fedora as free-software
				 * licenses even though they are not on the OSI list. */
				QStringLiteral("bsd-3-clause-clear"),
				QStringLiteral("bsd-4-clause"),
				QStringLiteral("cc0-1.0"),
				QStringLiteral("cc-by-4.0"),
				QStringLiteral("cc-by-sa-4.0"),
				QStringLiteral("unlicense"),
				QStringLiteral("vim"),
				QStringLiteral("wtfpl"),

				/* ══ Kite's own modular-GPL identifier ══════════ *
				 * Plugins shipped under the launcher's combined GPL +
				 * MMCO-Module-Exception SPDX expression are recognised
				 * as a single atomic identifier here so splitSpdx() does
				 * not have to special-case the "WITH" clause. */
				QStringLiteral("gpl-3.0-or-later with "
							   "licenseref-kite-mmco-module-exception-1.0"),
			};
			return ids;
		}

		/* Split an SPDX expression into the atomic license identifiers
		 * that compose it. Handles AND / OR / WITH operators by simple
		 * tokenisation — we don't try to fully parse precedence. */
		QStringList splitSpdx(const QString& expr)
		{
			QString s = expr.toLower().trimmed();
			// Drop parentheses
			s.replace('(', ' ');
			s.replace(')', ' ');

			QStringList atoms;
			QString current;
			const auto flush = [&]() {
				QString a = current.trimmed();
				if (!a.isEmpty())
					atoms.append(a);
				current.clear();
			};

			const QStringList tokens =
				s.split(QRegularExpression(R"(\s+)"), Qt::SkipEmptyParts);
			for (int i = 0; i < tokens.size(); ++i) {
				const QString& t = tokens[i];
				if (t == QStringLiteral("or") || t == QStringLiteral("and")) {
					flush();
					continue;
				}
				if (t == QStringLiteral("with")) {
					// keep accumulating into current so "GPL-3.0 WITH foo"
					// stays a single atom
					current += QLatin1Char(' ');
					current += t;
					current += QLatin1Char(' ');
					continue;
				}
				if (!current.isEmpty())
					current += QLatin1Char(' ');
				current += t;
			}
			flush();
			return atoms;
		}
	} // namespace

	void setKeyringPath(const QString& path)
	{
		QMutexLocker lock(&g_mutex);
		if (path == g_keyringPath)
			return;
		g_keyringPath = path;
#if KITE_PLUGIN_SIGNATURES
		// Drop the cached context so the next verify() rebuilds it
		// against the new home directory.
		g_gpgCtx.reset();
		g_gpgCtxHome.clear();
#endif
		// Keyring change is rare (once at startup), but if it does
		// happen we have to invalidate the cache too — a different
		// keyring may legitimately reach a different verdict for the
		// exact same payload bytes.
		g_cache.clear();
		g_cacheDirty = true;
	}

	void setCachePath(const QString& path)
	{
		QMutexLocker lock(&g_mutex);
		if (path == g_cachePath)
			return;
		g_cachePath = path;
		loadCacheLocked();
	}

	void flushCache()
	{
		QMutexLocker lock(&g_mutex);
		saveCacheLocked();
	}

	bool isOpenSourceLicense(const QString& spdxLicense)
	{
		if (spdxLicense.trimmed().isEmpty())
			return false;

		const QStringList atoms = splitSpdx(spdxLicense);
		const QSet<QString>& whitelist = ossSpdxIds();

		for (const QString& atom : atoms) {
			if (whitelist.contains(atom))
				return true;
			// Also accept a bare "with"-prefixed atom whose head id is OSS
			// (e.g. "gpl-3.0-or-later with classpath-exception-2.0").
			const int withIdx = atom.indexOf(QStringLiteral(" with "));
			if (withIdx > 0) {
				const QString head = atom.left(withIdx).trimmed();
				if (whitelist.contains(head))
					return true;
			}
		}
		return false;
	}

	ExtractedTrailer extractTrailer(const QString& filePath)
	{
		ExtractedTrailer out;

		QFile f(filePath);
		if (!f.open(QIODevice::ReadOnly))
			return out;

		const qint64 size = f.size();
		const qint64 footerSize =
			static_cast<qint64>(sizeof(quint64) + sizeof(quint32));
		if (size < footerSize)
			return out; // file too small to contain a trailer

		// Read the 12-byte footer (uint64 sigSize + uint32 magic).
		if (!f.seek(size - footerSize))
			return out;

		QByteArray footerBytes = f.read(footerSize);
		if (footerBytes.size() != footerSize)
			return out;

		// Read the two trailer fields as little-endian.
		const quint32 magic =
			qFromLittleEndian<quint32>(reinterpret_cast<const uchar*>(
				footerBytes.constData() + sizeof(quint64)));

		if (magic != static_cast<quint32>(MMCO_TRAILER_MAGIC))
			return out; // no trailer

		out.present = true;

		quint64 sigSize = qFromLittleEndian<quint64>(
			reinterpret_cast<const uchar*>(footerBytes.constData()));

		// Sanity: signature must fit before the footer.
		if (sigSize == 0 || static_cast<qint64>(sigSize) > size - footerSize) {
			out.malformed = true;
			return out;
		}

		const qint64 payloadSize =
			size - footerSize - static_cast<qint64>(sigSize);

		// Read payload then signature.
		if (!f.seek(0)) {
			out.malformed = true;
			return out;
		}
		out.payload = f.read(payloadSize);
		if (out.payload.size() != payloadSize) {
			out.malformed = true;
			out.payload.clear();
			return out;
		}
		out.signature = f.read(static_cast<qint64>(sigSize));
		if (out.signature.size() != static_cast<qint64>(sigSize)) {
			out.malformed = true;
			out.payload.clear();
			out.signature.clear();
			return out;
		}
		return out;
	}

	PluginSignatureState verify(const QByteArray& payload,
								const QByteArray& signature, QString& detail,
								QString& fingerprint)
	{
		detail.clear();
		fingerprint.clear();

#if !KITE_PLUGIN_SIGNATURES
		(void)payload;
		(void)signature;
		// Stub build (MSVC): no GpgME backend is linked. Report an
		// engine-unavailable Error so the license/signature policy
		// treats the module the same way it would on a Linux box
		// without a working keyring. OSS modules still load; non-OSS
		// modules are rejected with SignatureRequired.
		detail = QStringLiteral(
			"GPG verification is not available in this build of Kite");
		return PluginSignatureState::Error;
#else
		ensureGpgmeInit();

		// Engine check is cheap once gpg-agent is up; GpgME caches the
		// answer internally. Still worth keeping because it gives us a
		// readable error message if the agent is unreachable on this
		// system at all.
		const auto err = GpgME::checkEngine(GpgME::OpenPGP);
		if (err) {
			detail = QStringLiteral("GpgME engine unavailable: %1")
						 .arg(QString::fromUtf8(err.asString()));
			return PluginSignatureState::Error;
		}

		// Acquire the long-lived GpgME::Context under the mutex.
		// gpgContextLocked() builds it on first use and rebuilds it
		// only if the keyring path has been re-configured since the
		// last call. Verification itself runs inside the lock too,
		// because GpgME::Context instances are not thread-safe — but
		// the work is short (single RPC to gpg-agent) and the caller
		// gains far more from parallel I/O on the OUTER side (reading
		// trailers off disk) than it could ever gain from concurrent
		// GpgME calls.
		QMutexLocker lock(&g_mutex);
		GpgME::Context* ctx = gpgContextLocked();
		if (!ctx) {
			detail = QStringLiteral("Failed to create GpgME context");
			return PluginSignatureState::Error;
		}

		GpgME::Data sigData(signature.constData(),
							static_cast<size_t>(signature.size()),
							/* copy */ false);
		GpgME::Data payloadData(payload.constData(),
								static_cast<size_t>(payload.size()),
								/* copy */ false);

		const GpgME::VerificationResult result =
			ctx->verifyDetachedSignature(sigData, payloadData);

		if (result.error()) {
			detail = QStringLiteral("Verification error: %1")
						 .arg(QString::fromUtf8(result.error().asString()));
			return PluginSignatureState::Error;
		}

		const auto sigs = result.signatures();
		if (sigs.empty()) {
			detail = QStringLiteral("Trailer contained no parsable signatures");
			return PluginSignatureState::BadSignature;
		}

		// We only honour the first signature.
		const auto& sig = sigs.front();
		if (sig.fingerprint())
			fingerprint = QString::fromUtf8(sig.fingerprint());

		const auto status = sig.status();
		if (status.code() != 0) {
			// status() is set for bad / expired / revoked signatures.
			detail = QStringLiteral("Bad signature: %1")
						 .arg(QString::fromUtf8(status.asString()));
			return PluginSignatureState::BadSignature;
		}

		const auto summary = sig.summary();
		if (summary & GpgME::Signature::Red) {
			detail = QStringLiteral("Signature failed verification");
			return PluginSignatureState::BadSignature;
		}

		// Green = good and trusted. Valid = good but trust below threshold.
		if (summary & GpgME::Signature::Green) {
			detail = QStringLiteral("Good signature");
			return PluginSignatureState::Valid;
		}
		if (summary & GpgME::Signature::Valid) {
			detail = QStringLiteral("Good signature (trust below threshold)");
			return PluginSignatureState::Valid;
		}
		if (summary & GpgME::Signature::KeyMissing) {
			detail = QStringLiteral(
				"Signing key not present in the trusted keyring");
			return PluginSignatureState::Untrusted;
		}
		// Anything else we treat as "verified by an unknown / untrusted key".
		detail =
			QStringLiteral("Signature present but signing key not trusted");
		return PluginSignatureState::Untrusted;
#endif // KITE_PLUGIN_SIGNATURES
	}

	PluginSignatureState verifyFile(const QString& filePath, QString& detail,
									QString& fingerprint, bool bypassCache)
	{
		// ── Cache lookup ───────────────────────────────────────
		// Build the (path, size, mtime_ms) key from the file's metadata.
		// stat()-level metadata is far cheaper than reading the whole
		// payload + signature and shelling out to gpg-agent — on a
		// modern SSD it's a single inode lookup.
		QFileInfo fi(filePath);
		CacheKey key;
		key.path = fi.absoluteFilePath();
		key.size = fi.size();
		key.mtimeMs = fi.lastModified().toMSecsSinceEpoch();

		if (!bypassCache) {
			QMutexLocker lock(&g_mutex);
			auto it = g_cache.constFind(key);
			if (it != g_cache.constEnd()) {
				detail = it->detail;
				fingerprint = it->fingerprint;
				return it->state;
			}
		}

		// ── Slow path: read the trailer and verify ─────────────
		ExtractedTrailer trailer = extractTrailer(filePath);

		PluginSignatureState state;
		if (!trailer.present) {
			detail.clear();
			fingerprint.clear();
			state = PluginSignatureState::Absent;
		} else if (trailer.malformed) {
			detail = QStringLiteral("Malformed signature trailer");
			fingerprint.clear();
			state = PluginSignatureState::Malformed;
		} else {
			state =
				verify(trailer.payload, trailer.signature, detail, fingerprint);
		}

		// ── Memoise the result ─────────────────────────────────
		// Every terminal state is cached, including failures: a
		// BadSignature outcome must remain BadSignature across
		// launcher restarts unless the file is actually rewritten
		// (which would change mtime + size and bust the entry).
		// Error states (gpg-agent down, etc.) are NOT cached so a
		// transient agent failure doesn't poison the cache.
		if (state != PluginSignatureState::Error) {
			QMutexLocker lock(&g_mutex);
			CacheEntry entry;
			entry.state = state;
			entry.detail = detail;
			entry.fingerprint = fingerprint;
			g_cache.insert(key, entry);
			g_cacheDirty = true;
		}
		return state;
	}

	const char* stateLabel(PluginSignatureState state)
	{
		switch (state) {
			case PluginSignatureState::NotChecked:
				return "Not checked";
			case PluginSignatureState::Absent:
				return "Unsigned";
			case PluginSignatureState::Valid:
				return "Signed (trusted)";
			case PluginSignatureState::Untrusted:
				return "Signed (untrusted key)";
			case PluginSignatureState::BadSignature:
				return "Bad signature";
			case PluginSignatureState::Malformed:
				return "Malformed signature";
			case PluginSignatureState::Error:
				return "Verification error";
		}
		return "Unknown";
	}

} // namespace PluginSignature
