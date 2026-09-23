/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileContributor: Project Tick
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

#include <QTest>

#include <QSignalSpy>
#include <QVariant>
#include <QVariantMap>

#include "plugin/PluginSurfaceModel.h"

namespace
{
	/* Feeds a canned SurfaceInfo list instead of reading a real
	 * PluginManager's — see PluginSurfaceModel.h's fetchSurfaces() doc
	 * comment for why this is the model's test seam. Built with a null
	 * manager: nothing here ever needs one, since fetchSurfaces() is
	 * fully overridden and sendEvent()/deliverUiEvent() is exercised
	 * separately as a safe no-op. */
	class TestSurfaceModel : public PluginSurfaceModel
	{
	  public:
		TestSurfaceModel() : PluginSurfaceModel(nullptr) {}

		void setCanned(QList<PluginManager::SurfaceInfo> infos)
		{
			m_canned = std::move(infos);
			refresh();
		}

	  protected:
		QList<PluginManager::SurfaceInfo> fetchSurfaces() const override
		{
			return m_canned;
		}

	  private:
		QList<PluginManager::SurfaceInfo> m_canned;
	};

	PluginManager::SurfaceInfo makeInfo(const QString& id, const QString& doc)
	{
		PluginManager::SurfaceInfo info;
		info.surfaceId = id;
		info.anchor = 0;
		info.anchorContext = QStringLiteral("inst-1");
		info.title = QStringLiteral("Title-") + id;
		info.iconName = QStringLiteral("icon");
		info.document = doc;
		return info;
	}

	const char* kMinimalDoc =
		"{\"type\":\"mmco-ui/1\",\"root\":{\"type\":\"column\",\"id\":\"root\","
		"\"children\":[]}}";
} // namespace

class PluginSurfaceModelTest : public QObject
{
	Q_OBJECT

  private slots:
	void rowsAndRoles();
	void revisionTracksDocumentChanges();
	void propertiesResetOnChange();
	void sendEventWithoutManagerIsNoop();

	void valueToJson_data();
	void valueToJson();
};

void PluginSurfaceModelTest::rowsAndRoles()
{
	TestSurfaceModel model;
	model.setCanned({ makeInfo(QStringLiteral("sf-1"), QString::fromUtf8(kMinimalDoc)) });

	QCOMPARE(model.rowCount(), 1);
	const QModelIndex idx = model.index(0, 0);
	QVERIFY(idx.isValid());

	QCOMPARE(model.data(idx, PluginSurfaceModel::SurfaceIdRole).toString(),
			QStringLiteral("sf-1"));
	QCOMPARE(model.data(idx, PluginSurfaceModel::TitleRole).toString(),
			QStringLiteral("Title-sf-1"));
	QCOMPARE(model.data(idx, PluginSurfaceModel::IconNameRole).toString(),
			QStringLiteral("icon"));
	QCOMPARE(model.data(idx, PluginSurfaceModel::AnchorContextRole).toString(),
			QStringLiteral("inst-1"));

	// The document comes back as a QVariantMap QML can walk straight into
	// root/type/id/props/children without any JSON parsing of its own.
	const QVariantMap doc =
		model.data(idx, PluginSurfaceModel::DocumentRole).toMap();
	QCOMPARE(doc.value(QStringLiteral("type")).toString(),
			QStringLiteral("mmco-ui/1"));
	const QVariantMap root = doc.value(QStringLiteral("root")).toMap();
	QCOMPARE(root.value(QStringLiteral("type")).toString(),
			QStringLiteral("column"));
	QCOMPARE(root.value(QStringLiteral("id")).toString(), QStringLiteral("root"));

	// A brand new surface id starts at revision 1, not 0 -- "has a
	// document" and "revision assigned" happen together.
	QCOMPARE(model.data(idx, PluginSurfaceModel::RevisionRole).toInt(), 1);

	// roleNames() names every role a QML delegate would bind to by name.
	const auto roles = model.roleNames();
	QCOMPARE(roles.value(PluginSurfaceModel::SurfaceIdRole),
			QByteArray("surfaceId"));
	QCOMPARE(roles.value(PluginSurfaceModel::DocumentRole),
			QByteArray("document"));
	QCOMPARE(roles.value(PluginSurfaceModel::RevisionRole),
			QByteArray("revision"));
}

void PluginSurfaceModelTest::revisionTracksDocumentChanges()
{
	TestSurfaceModel model;
	const QString docA = QStringLiteral("{\"a\":1}");
	const QString docB = QStringLiteral("{\"a\":2}");

	model.setCanned({ makeInfo(QStringLiteral("sf-1"), docA) });
	QCOMPARE(model.data(model.index(0, 0), PluginSurfaceModel::RevisionRole).toInt(),
			1);

	// Re-reading the same document text (a surfacesChanged() fired by some
	// unrelated surface, say) must not bump a revision nothing changed.
	model.setCanned({ makeInfo(QStringLiteral("sf-1"), docA) });
	QCOMPARE(model.data(model.index(0, 0), PluginSurfaceModel::RevisionRole).toInt(),
			1);

	// An actual document change (ui_surface_update/_set/_set_rows) bumps it.
	model.setCanned({ makeInfo(QStringLiteral("sf-1"), docB) });
	QCOMPARE(model.data(model.index(0, 0), PluginSurfaceModel::RevisionRole).toInt(),
			2);

	// The surface disappearing (ui_surface_destroy) drops its bookkeeping;
	// a later surface reusing the id (never happens in practice -- ids are
	// sequential -- but nothing stops a test) starts fresh.
	model.setCanned({});
	QCOMPARE(model.rowCount(), 0);
	model.setCanned({ makeInfo(QStringLiteral("sf-1"), docB) });
	QCOMPARE(model.data(model.index(0, 0), PluginSurfaceModel::RevisionRole).toInt(),
			1);
}

void PluginSurfaceModelTest::propertiesResetOnChange()
{
	TestSurfaceModel model;
	QSignalSpy anchorSpy(&model, &PluginSurfaceModel::anchorChanged);
	QSignalSpy contextSpy(&model, &PluginSurfaceModel::anchorContextChanged);

	QCOMPARE(model.anchor(), -1);
	QVERIFY(model.anchorContext().isNull());

	model.setAnchor(2);
	QCOMPARE(model.anchor(), 2);
	QCOMPARE(anchorSpy.count(), 1);

	// Setting the same value again is a no-op -- no redundant signal / reset.
	model.setAnchor(2);
	QCOMPARE(anchorSpy.count(), 1);

	model.setAnchorContext(QStringLiteral("inst-2"));
	QCOMPARE(model.anchorContext(), QStringLiteral("inst-2"));
	QCOMPARE(contextSpy.count(), 1);
}

void PluginSurfaceModelTest::sendEventWithoutManagerIsNoop()
{
	TestSurfaceModel model;
	// Must not crash: sendEvent() guards on a null manager the same way
	// PluginManager::deliverUiEvent() guards on an unknown surfaceId.
	model.sendEvent(QStringLiteral("sf-1"), QStringLiteral("node"),
					QStringLiteral("click"), QVariant());
}

void PluginSurfaceModelTest::valueToJson_data()
{
	QTest::addColumn<QVariant>("value");
	QTest::addColumn<QString>("expected");

	// Button click: no value at all.
	QTest::newRow("invalid") << QVariant() << QString();
	// Toggle change.
	QTest::newRow("bool_true") << QVariant(true) << QStringLiteral("true");
	QTest::newRow("bool_false") << QVariant(false) << QStringLiteral("false");
	// number_field change.
	QTest::newRow("int") << QVariant(42) << QStringLiteral("42");
	QTest::newRow("negative_int") << QVariant(-7) << QStringLiteral("-7");
	QTest::newRow("double") << QVariant(3.5) << QStringLiteral("3.5");
	// text_field / choice change, list select/activate (a row id).
	QTest::newRow("string") << QVariant(QStringLiteral("abc"))
							 << QStringLiteral("\"abc\"");
	QTest::newRow("string_needs_escaping")
		<< QVariant(QStringLiteral("say \"hi\"")) << QString::fromUtf8(
													   "\"say \\\"hi\\\"\"");
	QTest::newRow("empty_string") << QVariant(QStringLiteral(""))
								  << QStringLiteral("\"\"");
}

void PluginSurfaceModelTest::valueToJson()
{
	QFETCH(QVariant, value);
	QFETCH(QString, expected);
	QCOMPARE(PluginSurfaceModel::valueToJson(value), expected);
}

QTEST_GUILESS_MAIN(PluginSurfaceModelTest)

#include "PluginSurfaceModel_test.moc"
