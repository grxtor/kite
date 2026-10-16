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

#include "plugin/PluginListModel.h"

#include <QStringList>

#include "plugin/PluginManager.h"

PluginListModel::PluginListModel(PluginManager* manager, QObject* parent)
	: QAbstractListModel(parent), m_manager(manager)
{
	refresh();
}

int PluginListModel::rowCount(const QModelIndex& parent) const
{
	return parent.isValid() ? 0 : m_modules.size();
}

QVariant PluginListModel::data(const QModelIndex& index, int role) const
{
	if (!index.isValid() || index.row() < 0 || index.row() >= m_modules.size()) {
		return QVariant();
	}
	const PluginMetadata& mod = m_modules.at(index.row());
	const bool superseded =
		mod.disableReason == PluginDisableReason::SupersededByCore;

	switch (role) {
		case Qt::DisplayRole:
		case NameRole:
			return mod.name;
		case VersionRole:
			return mod.version;
		case AuthorRole:
			return mod.author;
		case DescriptionRole:
			return mod.description;
		case LicenseRole:
			return mod.license;
		case SourceUrlRole:
			return mod.codeLink;
		case SignatureRole:
			return signatureLabel(mod.signatureState);
		case SignedRole:
			return mod.signatureState == PluginSignatureState::Valid;
		case SigningKeyRole:
			return mod.signatureFingerprint;
		case StatusRole:
			if (mod.disabled) {
				return pluginDisableReasonLabel(mod.disableReason);
			}
			return mod.initialized ? tr("Loaded") : tr("Not loaded");
		case StatusKindRole:
			if (mod.disabled) {
				return QStringLiteral("disabled");
			}
			return mod.initialized ? QStringLiteral("loaded")
								   : QStringLiteral("discovered");
		case StatusDetailRole:
			return mod.disabled ? mod.disableDetail : QString();
		case EnabledRole:
			return !superseded && !isUserDisabled(mod.name);
		case CanToggleRole:
			return !superseded;
		case DependenciesRole: {
			// The same wording PluginsDialog's details pane uses.
			QStringList parts;
			for (const auto& dependency : mod.dependencies) {
				QString part = dependency.name;
				if (!dependency.minVersion.isEmpty()) {
					part += QStringLiteral(" >= %1").arg(dependency.minVersion);
				}
				if (dependency.optional) {
					part += tr(" (optional)");
				}
				parts.append(part);
			}
			return parts.join(QStringLiteral(", "));
		}
		default:
			return QVariant();
	}
}

QHash<int, QByteArray> PluginListModel::roleNames() const
{
	return {
		{ NameRole, "name" },
		{ VersionRole, "version" },
		{ AuthorRole, "author" },
		{ DescriptionRole, "description" },
		{ LicenseRole, "license" },
		{ SourceUrlRole, "sourceUrl" },
		{ SignatureRole, "signature" },
		{ SignedRole, "signed" },
		{ SigningKeyRole, "signingKey" },
		{ StatusRole, "status" },
		{ StatusKindRole, "statusKind" },
		{ StatusDetailRole, "statusDetail" },
		{ EnabledRole, "enabled" },
		{ CanToggleRole, "canToggle" },
		{ DependenciesRole, "dependencies" },
	};
}

int PluginListModel::count() const
{
	return m_modules.size();
}

bool PluginListModel::restartNeeded() const
{
	return m_restartNeeded;
}

void PluginListModel::setEnabled(int row, bool enabled)
{
	if (row < 0 || row >= m_modules.size()) {
		return;
	}
	const PluginMetadata& mod = m_modules.at(row);
	if (mod.disableReason == PluginDisableReason::SupersededByCore ||
		isUserDisabled(mod.name) == !enabled) {
		return;
	}
	setUserDisabled(mod.name, !enabled);
	const QModelIndex changed = index(row, 0);
	emit dataChanged(changed, changed, { EnabledRole });
	if (!m_restartNeeded) {
		m_restartNeeded = true;
		emit restartNeededChanged();
	}
}

void PluginListModel::refresh()
{
	const int before = m_modules.size();
	beginResetModel();
	m_modules = fetchModules();
	endResetModel();
	if (m_modules.size() != before) {
		emit countChanged();
	}
}

QString PluginListModel::signatureLabel(PluginSignatureState state)
{
	switch (state) {
		case PluginSignatureState::Valid:
			return tr("Signed");
		case PluginSignatureState::Untrusted:
			return tr("Signed by an unknown key");
		case PluginSignatureState::Absent:
			return tr("Unsigned");
		case PluginSignatureState::BadSignature:
		case PluginSignatureState::Malformed:
			return tr("Bad signature");
		case PluginSignatureState::Error:
			return tr("Signature not checked");
		case PluginSignatureState::NotChecked:
			break;
	}
	return tr("Signature not checked");
}

QVector<PluginMetadata> PluginListModel::fetchModules() const
{
	return m_manager ? m_manager->modules() : QVector<PluginMetadata>();
}

bool PluginListModel::isUserDisabled(const QString& name) const
{
	return m_manager && m_manager->isModuleDisabled(name);
}

void PluginListModel::setUserDisabled(const QString& name, bool disabled)
{
	if (m_manager) {
		m_manager->setModuleDisabled(name, disabled);
	}
}
