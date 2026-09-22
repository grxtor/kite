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

#pragma once

#include <QAbstractListModel>
#include <QString>
#include "MessageLevel.h"

class LogModel : public QAbstractListModel
{
	Q_OBJECT
  public:
	explicit LogModel(QObject* parent = 0);

	int rowCount(const QModelIndex& parent = QModelIndex()) const;
	QVariant data(const QModelIndex& index, int role) const;

	void append(MessageLevel::Enum, QString line);
	void clear();

	void suspend(bool suspend);
	bool suspended();

	QString toPlainText();

	int getMaxLines();
	void setMaxLines(int maxLines);
	void setStopOnOverflow(bool stop);
	void setOverflowMessage(const QString& overflowMessage);

	void setLineWrap(bool state);
	bool wrapLines() const;

	// LineRole is separate from the inherited "display"/"edit" role names
	// so QML can name the text without colliding with them.
	enum Roles { LevelRole = Qt::UserRole, LineRole };

	QHash<int, QByteArray> roleNames() const;

  private /* types */:
	struct entry {
		MessageLevel::Enum level;
		QString line;
	};

  private: /* data */
	QVector<entry> m_content;
	int m_maxLines = 1000;
	// first line in the circular buffer
	int m_firstLine = 0;
	// number of lines occupied in the circular buffer
	int m_numLines = 0;
	bool m_stopOnOverflow = false;
	QString m_overflowMessage = "OVERFLOW";
	bool m_suspended = false;
	bool m_lineWrap = true;

  private:
	Q_DISABLE_COPY(LogModel)
};
