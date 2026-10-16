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

#include <QSignalSpy>
#include <QSet>
#include <QTest>

#include "plugin/PluginListModel.h"

namespace
{
	/* Canned modules and an in-memory "disabled" set instead of a real
	 * PluginManager -- the same test seam PluginSurfaceModel_test uses. */
	class TestPluginListModel : public PluginListModel
	{
	  public:
		TestPluginListModel() : PluginListModel(nullptr) {}

		void setCanned(QVector<PluginMetadata> modules)
		{
			m_canned = std::move(modules);
			refresh();
		}
		QSet<QString> disabled;

	  protected:
		QVector<PluginMetadata> fetchModules() const override
		{
			return m_canned;
		}
		bool isUserDisabled(const QString& name) const override
		{
			return disabled.contains(name);
		}
		void setUserDisabled(const QString& name, bool off) override
		{
			if (off)
				disabled.insert(name);
			else
				disabled.remove(name);
		}

	  private:
		QVector<PluginMetadata> m_canned;
	};

	PluginMetadata module(const QString& name)
	{
		PluginMetadata mod;
		mod.name = name;
		mod.version = QStringLiteral("1.2");
		mod.license = QStringLiteral("MIT");
		mod.initialized = true;
		mod.signatureState = PluginSignatureState::Valid;
		return mod;
	}
} // namespace

class PluginListModelTest : public QObject
{
	Q_OBJECT

  private slots:
	void aNullManagerListsNothing()
	{
		PluginListModel model(nullptr);
		QCOMPARE(model.count(), 0);
		QVERIFY(!model.restartNeeded());
	}

	void rowsDescribeEachModule()
	{
		TestPluginListModel model;
		auto loaded = module(QStringLiteral("Loaded"));
		loaded.dependencies.append({ QStringLiteral("Core"), QStringLiteral("2.0"), false });
		loaded.dependencies.append({ QStringLiteral("Extra"), QString(), true });
		auto blocked = module(QStringLiteral("Blocked"));
		blocked.initialized = false;
		blocked.disabled = true;
		blocked.disableReason = PluginDisableReason::DependencyMissing;
		blocked.disableDetail = QStringLiteral("needs Core");
		blocked.signatureState = PluginSignatureState::Absent;
		model.setCanned({ loaded, blocked });

		QCOMPARE(model.count(), 2);
		const QModelIndex first = model.index(0, 0);
		QCOMPARE(model.data(first, PluginListModel::StatusKindRole).toString(),
				 QStringLiteral("loaded"));
		QVERIFY(model.data(first, PluginListModel::SignedRole).toBool());
		QCOMPARE(model.data(first, PluginListModel::DependenciesRole).toString(),
				 QStringLiteral("Core >= 2.0, Extra (optional)"));
		QVERIFY(model.data(first, PluginListModel::EnabledRole).toBool());

		const QModelIndex second = model.index(1, 0);
		QCOMPARE(model.data(second, PluginListModel::StatusKindRole).toString(),
				 QStringLiteral("disabled"));
		QCOMPARE(model.data(second, PluginListModel::StatusRole).toString(),
				 pluginDisableReasonLabel(PluginDisableReason::DependencyMissing));
		QCOMPARE(model.data(second, PluginListModel::StatusDetailRole).toString(),
				 QStringLiteral("needs Core"));
		QVERIFY(!model.data(second, PluginListModel::SignedRole).toBool());
	}

	// Switching remembers the wish and says a restart is needed.
	void setEnabledRemembersTheWish()
	{
		TestPluginListModel model;
		model.setCanned({ module(QStringLiteral("A")) });
		QSignalSpy restart(&model, &PluginListModel::restartNeededChanged);

		model.setEnabled(0, false);
		QVERIFY(model.disabled.contains(QStringLiteral("A")));
		QVERIFY(!model.data(model.index(0, 0), PluginListModel::EnabledRole).toBool());
		QVERIFY(model.restartNeeded());
		QCOMPARE(restart.count(), 1);

		model.setEnabled(0, true);
		QVERIFY(!model.disabled.contains(QStringLiteral("A")));
		QCOMPARE(restart.count(), 1); // still needed, not signalled again
	}

	// A plugin the launcher has absorbed cannot be switched back on.
	void supersededModulesCannotBeToggled()
	{
		TestPluginListModel model;
		auto absorbed = module(QStringLiteral("Absorbed"));
		absorbed.disabled = true;
		absorbed.disableReason = PluginDisableReason::SupersededByCore;
		model.setCanned({ absorbed });

		const QModelIndex row = model.index(0, 0);
		QVERIFY(!model.data(row, PluginListModel::CanToggleRole).toBool());
		model.setEnabled(0, false);
		QVERIFY(model.disabled.isEmpty());
		QVERIFY(!model.restartNeeded());
	}
};

QTEST_GUILESS_MAIN(PluginListModelTest)

#include "PluginListModel_test.moc"
