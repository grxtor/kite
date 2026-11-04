/* SPDX-FileCopyrightText: 2026 grxtor
 * SPDX-FileContributor: grxtor
 * SPDX-License-Identifier: Apache-2.0
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

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTest>

#include <monocypher-ed25519.h>

#include "updater/UpdateManifest.h"

namespace
{
	/* Made by packaging/update-feed/kite_update_feed.py (its own Ed25519) with the seed 32..63,
	 * so this also proves the release workflow's signer and the updater's check agree. */
	const QByteArray kPythonManifest =
		"{\n  \"format\": \"kite-update-manifest\",\n  \"schema\": 1,\n  \"name\": \"Kite\",\n"
		"  \"version\": \"1.3.0\",\n  \"tag\": \"v1.3.0\",\n  \"created\": \"2026-11-04T12:00:00Z\",\n"
		"  \"assets\": [\n    {\n      \"name\": \"Kite-Linux-Qt6-x86_64.AppImage\",\n"
		"      \"size\": 122040632,\n"
		"      \"sha256\": \"4873e29395725b1cb43fe615a9b413ddb674e906bd5774bc66c1b500f322839b\"\n"
		"    },\n    {\n      \"name\": \"Kite-Windows-MSVC-Qt6-Setup-v1.3.0.exe\",\n"
		"      \"size\": 48482383,\n"
		"      \"sha256\": \"395a6416a3b96393b57b15a2d91cd7d93ff0e42f2709847c09257d3158f3e71e\"\n"
		"    }\n  ]\n}\n";
	const QByteArray kPythonSignature =
		"7Lc6j/A8/AwfPqBQqLdZE5nWcfirAQBxZvOWidWTuRmSskTLBIQ00Q9xnZ7hFsXTNHKGsbc/VxNpyTzTWV3jDg==\n";
	const QString kPythonPublicKey =
		QStringLiteral("Kay64UG8yvCyLhqU000LxzYeUm0L/hLIl5S8kyKWbdc=");

	const QString kHash =
		QStringLiteral("4873e29395725b1cb43fe615a9b413ddb674e906bd5774bc66c1b500f322839b");

	//! A key pair of our own, to sign manifests the release workflow would never write.
	struct Signer {
		uint8_t secret[64];
		uint8_t publicKey[32];

		Signer()
		{
			uint8_t seed[32];
			for (int i = 0; i < 32; ++i)
				seed[i] = static_cast<uint8_t>(100 + i);
			crypto_ed25519_key_pair(secret, publicKey, seed);
		}

		QString publicKeyBase64() const
		{
			return QString::fromLatin1(
				QByteArray(reinterpret_cast<const char*>(publicKey), 32)
					.toBase64());
		}

		QByteArray sign(const QByteArray& data) const
		{
			uint8_t signature[64];
			crypto_ed25519_sign(signature, secret,
								reinterpret_cast<const uint8_t*>(data.constData()),
								static_cast<size_t>(data.size()));
			return QByteArray(reinterpret_cast<const char*>(signature), 64)
				.toBase64();
		}
	};

	QJsonObject asset(const QString& name, qint64 size = 10,
					  const QString& sha256 = kHash)
	{
		return QJsonObject{{"name", name}, {"size", size}, {"sha256", sha256}};
	}

	QByteArray manifestJson(const QJsonArray& assets,
							const QString& tag = QStringLiteral("v1.3.0"),
							int schema = 1)
	{
		const QJsonObject root{{"format", "kite-update-manifest"},
							   {"schema", schema},
							   {"name", "Kite"},
							   {"version", "1.3.0"},
							   {"tag", tag},
							   {"created", "2026-11-04T12:00:00Z"},
							   {"assets", assets}};
		return QJsonDocument(root).toJson();
	}

} // namespace

class UpdateManifestTest : public QObject
{
	Q_OBJECT

  private slots:
	//! What the release workflow signs, the updater accepts.
	void test_manifestFromTheReleaseScriptLoads()
	{
		QString error;
		const auto manifest =
			UpdateManifest::load(kPythonManifest, kPythonSignature,
								 kPythonPublicKey, QStringLiteral("v1.3.0"), &error);
		QVERIFY2(manifest.has_value(), qPrintable(error));
		QCOMPARE(manifest->version, QStringLiteral("1.3.0"));
		QCOMPARE(manifest->tag, QStringLiteral("v1.3.0"));
		QCOMPARE(manifest->assets.size(), 2);

		const auto setup = manifest->asset(
			QStringLiteral("Kite-Windows-MSVC-Qt6-Setup-v1.3.0.exe"));
		QVERIFY(setup.has_value());
		QCOMPARE(setup->size, 48482383);
		QCOMPARE(setup->sha256,
				 QStringLiteral("395a6416a3b96393b57b15a2d91cd7d93ff0e42f2709847c09257d3158f3e71e"));
		QVERIFY(!manifest->asset(QStringLiteral("Kite-Windows-MSVC-Qt6-v1.3.0.zip")));
	}

	//! One byte changed anywhere, and the signature no longer covers it: whitespace counts too.
	void test_anyChangeBreaksTheSignature()
	{
		QByteArray changed = kPythonManifest;
		changed.replace("48482383", "48482384");
		QString error;
		QVERIFY(!UpdateManifest::load(changed, kPythonSignature, kPythonPublicKey,
									  QStringLiteral("v1.3.0"), &error));
		QVERIFY2(error.contains(QLatin1String("does not verify")), qPrintable(error));

		QVERIFY(!UpdateManifest::load(kPythonManifest + "\n", kPythonSignature,
									  kPythonPublicKey, QStringLiteral("v1.3.0"), &error));
	}

	//! Signed by anyone but the release key: refused, however well-formed.
	void test_otherKeyIsRefused()
	{
		const Signer stranger;
		const QByteArray json = manifestJson({asset(QStringLiteral("Kite.zip"))});
		QString error;
		QVERIFY(!UpdateManifest::load(json, stranger.sign(json), kPythonPublicKey,
									  QStringLiteral("v1.3.0"), &error));
		QVERIFY(!UpdateManifest::load(kPythonManifest, kPythonSignature,
									  stranger.publicKeyBase64(),
									  QStringLiteral("v1.3.0"), &error));
	}

	void test_malformedKeyOrSignature_data()
	{
		QTest::addColumn<QByteArray>("signature");
		QTest::addColumn<QString>("key");
		QTest::addColumn<QString>("message");

		QTest::newRow("empty signature") << QByteArray() << kPythonPublicKey
										 << QStringLiteral("not a base64");
		QTest::newRow("not base64") << QByteArray("!!!!") << kPythonPublicKey
									<< QStringLiteral("not a base64");
		QTest::newRow("short signature")
			<< QByteArray(32, 'A').toBase64() << kPythonPublicKey
			<< QStringLiteral("not a base64");
		QTest::newRow("empty key") << kPythonSignature << QString()
								   << QStringLiteral("built-in update key");
		QTest::newRow("short key") << kPythonSignature
								   << QString::fromLatin1(QByteArray(16, 'k').toBase64())
								   << QStringLiteral("built-in update key");
	}

	void test_malformedKeyOrSignature()
	{
		QFETCH(QByteArray, signature);
		QFETCH(QString, key);
		QFETCH(QString, message);
		QString error;
		QVERIFY(!UpdateManifest::load(kPythonManifest, signature, key,
									  QStringLiteral("v1.3.0"), &error));
		QVERIFY2(error.contains(message), qPrintable(error));
	}

	/*! A validly signed manifest of an older release cannot be passed off as a newer release's: the
	 * files it vouches for are the old ones. */
	void test_manifestOfAnotherReleaseIsRefused()
	{
		QString error;
		QVERIFY(!UpdateManifest::load(kPythonManifest, kPythonSignature,
									  kPythonPublicKey, QStringLiteral("v1.4.0"), &error));
		QVERIFY2(error.contains(QLatin1String("belongs to release v1.3.0, not v1.4.0")),
				 qPrintable(error));
	}

	void test_signedButMalformed_data()
	{
		QTest::addColumn<QByteArray>("json");
		QTest::addColumn<QString>("message");

		QTest::newRow("not JSON") << QByteArray("kite") << QStringLiteral("not a JSON object");
		QTest::newRow("array") << QByteArray("[]") << QStringLiteral("not a JSON object");
		QTest::newRow("future schema")
			<< manifestJson({asset(QStringLiteral("a.zip"))}, QStringLiteral("v1.3.0"), 2)
			<< QStringLiteral("schema 2");
		QTest::newRow("no files") << manifestJson({}) << QStringLiteral("lists no files");
		QTest::newRow("uppercase hash")
			<< manifestJson({asset(QStringLiteral("a.zip"), 10, kHash.toUpper())})
			<< QStringLiteral("malformed entry for 'a.zip'");
		QTest::newRow("short hash")
			<< manifestJson({asset(QStringLiteral("a.zip"), 10, kHash.left(10))})
			<< QStringLiteral("malformed entry");
		QTest::newRow("negative size")
			<< manifestJson({asset(QStringLiteral("a.zip"), -1)})
			<< QStringLiteral("malformed entry");
		QTest::newRow("path") << manifestJson({asset(QStringLiteral("../kite.exe"))})
							  << QStringLiteral("malformed entry");
		QTest::newRow("windows path")
			<< manifestJson({asset(QStringLiteral("bin\\kite.exe"))})
			<< QStringLiteral("malformed entry");
		QTest::newRow("twice") << manifestJson({asset(QStringLiteral("a.zip")),
												asset(QStringLiteral("a.zip"))})
							   << QStringLiteral("lists 'a.zip' twice");
	}

	//! Signed by the right key, yet not something to install from.
	void test_signedButMalformed()
	{
		QFETCH(QByteArray, json);
		QFETCH(QString, message);
		const Signer release;
		QString error;
		QVERIFY(!UpdateManifest::load(json, release.sign(json), release.publicKeyBase64(),
									  QStringLiteral("v1.3.0"), &error));
		QVERIFY2(error.contains(message), qPrintable(error));
	}

	void test_oversizedManifestIsRefused()
	{
		const Signer release;
		const QByteArray huge(UpdateManifest::kMaxBytes + 1, ' ');
		QString error;
		QVERIFY(!UpdateManifest::load(huge, release.sign(huge), release.publicKeyBase64(),
									  QStringLiteral("v1.3.0"), &error));
		QVERIFY2(error.contains(QLatin1String("too large")), qPrintable(error));
	}

	//! Large sizes survive: the AppImage is over 100 MiB and a future one may pass 2 GiB.
	void test_largeSizes()
	{
		const Signer release;
		const qint64 big = 5'000'000'000LL;
		const QByteArray json = manifestJson({asset(QStringLiteral("Kite.AppImage"), big)});
		QString error;
		const auto manifest = UpdateManifest::load(
			json, release.sign(json), release.publicKeyBase64(), QStringLiteral("v1.3.0"),
			&error);
		QVERIFY2(manifest.has_value(), qPrintable(error));
		QCOMPARE(manifest->asset(QStringLiteral("Kite.AppImage"))->size, big);
	}
};

QTEST_GUILESS_MAIN(UpdateManifestTest)

#include "UpdateManifest_test.moc"
