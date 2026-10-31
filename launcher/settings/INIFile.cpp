/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileContributor: Project Tick
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright 2013-2021 MultiMC Contributors
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

#include "settings/INIFile.h"
#include <FileSystem.h>

#include <QFile>
#include <QTextStream>
#include <QStringList>
#include <QSaveFile>
#include <QDebug>

INIFile::INIFile() {}

QString INIFile::unescape(QString orig)
{
	QString out;
	QChar prev = QChar();
	for (auto c : orig) {
		if (prev == QLatin1Char('\\')) {
			if (c == 'n')
				out += '\n';
			else if (c == 't')
				out += '\t';
			else if (c == '#')
				out += '#';
			else
				out += c;
			prev = QChar();
		} else {
			if (c == '\\') {
				prev = c;
				continue;
			}
			out += c;
			prev = QChar();
		}
	}
	return out;
}

QString INIFile::escape(QString orig)
{
	QString out;
	for (auto c : orig) {
		if (c == '\n')
			out += "\\n";
		else if (c == '\t')
			out += "\\t";
		else if (c == '\\')
			out += "\\\\";
		else if (c == '#')
			out += "\\#";
		else
			out += c;
	}
	return out;
}

bool INIFile::saveFile(QString fileName)
{
	QByteArray outArray;
	for (Iterator iter = begin(); iter != end(); iter++) {
		QString value = iter.value().toString();
		value = escape(value);
		outArray.append(iter.key().toUtf8());
		outArray.append('=');
		outArray.append(value.toUtf8());
		outArray.append('\n');
	}

	try {
		FS::write(fileName, outArray);
	} catch (const Exception& e) {
		qCritical() << e.what();
		return false;
	}

	return true;
}

bool INIFile::loadFile(QString fileName)
{
	QFile file(fileName);
	if (!file.open(QIODevice::ReadOnly))
		return false;
	bool success = loadFile(file.readAll());
	file.close();
	return success;
}

bool INIFile::loadFile(QByteArray file)
{
	QTextStream in(file);

	QStringList lines = in.readAll().split('\n');
	for (int i = 0; i < lines.count(); i++) {
		QString& lineRaw = lines[i];
		int commentIndex = -1;
		// A backslash escapes the next character, so only a bare # starts a comment.
		for (int c = 0; c < lineRaw.size(); c++) {
			if (lineRaw.at(c) == '\\') {
				c++;
			} else if (lineRaw.at(c) == '#') {
				commentIndex = c;
				break;
			}
		}
		QString line =
			(commentIndex == -1 ? lineRaw : lineRaw.left(commentIndex)).trimmed();

		int eqPos = line.indexOf('=');
		if (eqPos == -1)
			continue;
		QString key = line.left(eqPos).trimmed();
		QString valueStr = line.right(line.length() - eqPos - 1).trimmed();

		valueStr = unescape(valueStr);

		QVariant value(valueStr);
		this->operator[](key) = value;
	}

	return true;
}

QVariant INIFile::get(QString key, QVariant def) const
{
	if (!this->contains(key))
		return def;
	else
		return this->operator[](key);
}

void INIFile::set(QString key, QVariant val)
{
	this->operator[](key) = val;
}
