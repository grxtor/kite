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

#include "LogModel.h"

LogModel::LogModel(QObject* parent) : QAbstractListModel(parent)
{
	m_content.resize(m_maxLines);
}

int LogModel::rowCount(const QModelIndex& parent) const
{
	if (parent.isValid())
		return 0;

	return m_numLines;
}

QVariant LogModel::data(const QModelIndex& index, int role) const
{
	if (index.row() < 0 || index.row() >= m_numLines)
		return QVariant();

	auto row = index.row();
	auto realRow = (row + m_firstLine) % m_maxLines;
	if (role == Qt::DisplayRole || role == Qt::EditRole) {
		return m_content[realRow].line;
	}
	if (role == LevelRole) {
		return m_content[realRow].level;
	}
	if (role == LineRole) {
		return m_content[realRow].line;
	}

	return QVariant();
}

QHash<int, QByteArray> LogModel::roleNames() const
{
	QHash<int, QByteArray> roles = QAbstractListModel::roleNames();
	roles.insert(LineRole, "line");
	roles.insert(LevelRole, "level");
	return roles;
}

void LogModel::append(MessageLevel::Enum level, QString line)
{
	if (m_suspended) {
		return;
	}
	int lineNum = (m_firstLine + m_numLines) % m_maxLines;
	// overflow
	if (m_numLines == m_maxLines) {
		if (m_stopOnOverflow) {
			// nothing more to do, the buffer is full
			return;
		}
		beginRemoveRows(QModelIndex(), 0, 0);
		m_firstLine = (m_firstLine + 1) % m_maxLines;
		m_numLines--;
		endRemoveRows();
	} else if (m_numLines == m_maxLines - 1 && m_stopOnOverflow) {
		level = MessageLevel::Fatal;
		line = m_overflowMessage;
	}
	beginInsertRows(QModelIndex(), m_numLines, m_numLines);
	m_numLines++;
	m_content[lineNum].level = level;
	m_content[lineNum].line = line;
	endInsertRows();
}

void LogModel::suspend(bool suspend)
{
	m_suspended = suspend;
}

bool LogModel::suspended()
{
	return m_suspended;
}

void LogModel::clear()
{
	beginResetModel();
	m_firstLine = 0;
	m_numLines = 0;
	endResetModel();
}

QString LogModel::toPlainText()
{
	QString out;
	out.reserve(m_numLines * 80);
	for (int i = 0; i < m_numLines; i++) {
		QString& line = m_content[(m_firstLine + i) % m_maxLines].line;
		out.append(line + '\n');
	}
	out.squeeze();
	return out;
}

void LogModel::setMaxLines(int maxLines)
{
	// no-op
	if (maxLines == m_maxLines) {
		return;
	}
	// if it all still fits in the buffer, just resize it
	if (m_firstLine + m_numLines < m_maxLines) {
		m_maxLines = maxLines;
		m_content.resize(maxLines);
		return;
	}
	// otherwise, we need to reorganize the data because it crosses the wrap
	// boundary
	QVector<entry> newContent;
	newContent.resize(maxLines);
	if (m_numLines <= maxLines) {
		// if it all fits in the new buffer, just copy it over
		for (int i = 0; i < m_numLines; i++) {
			newContent[i] = m_content[(m_firstLine + i) % m_maxLines];
		}
		m_content.swap(newContent);
	} else {
		// if it doesn't fit, part of the data needs to be thrown away (the
		// oldest log messages)
		int lead = m_numLines - maxLines;
		beginRemoveRows(QModelIndex(), 0, lead - 1);
		for (int i = 0; i < maxLines; i++) {
			newContent[i] = m_content[(m_firstLine + lead + i) % m_maxLines];
		}
		m_numLines = m_maxLines;
		m_content.swap(newContent);
		endRemoveRows();
	}
	m_firstLine = 0;
	m_maxLines = maxLines;
}

int LogModel::getMaxLines()
{
	return m_maxLines;
}

void LogModel::setStopOnOverflow(bool stop)
{
	m_stopOnOverflow = stop;
}

void LogModel::setOverflowMessage(const QString& overflowMessage)
{
	m_overflowMessage = overflowMessage;
}

void LogModel::setLineWrap(bool state)
{
	if (m_lineWrap != state) {
		m_lineWrap = state;
	}
}

bool LogModel::wrapLines() const
{
	return m_lineWrap;
}
