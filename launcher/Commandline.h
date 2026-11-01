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

#pragma once

#include <exception>
#include <stdexcept>

#include <QString>
#include <QVariant>
#include <QHash>
#include <QStringList>

namespace Commandline
{

	/** Splits a string into argv items like a shell would. */
	QStringList splitArgs(QString args);

	namespace FlagStyle
	{
		enum Enum {
			GNU,	 /**< --option and -o (GNU Style) */
			Unix,	 /**< -option and -o  (Unix Style) */
			Windows, /**< /option and /o  (Windows Style) */
#ifdef Q_OS_WIN32
			Default = Windows
#else
			Default = GNU
#endif
		};
	} // namespace FlagStyle

	namespace ArgumentStyle
	{
		enum Enum {
			Space,			/**< --option value */
			Equals,			/**< --option=value */
			SpaceAndEquals, /**< --option[= ]value */
#ifdef Q_OS_WIN32
			Default = Equals
#else
			Default = SpaceAndEquals
#endif
		};
	} // namespace ArgumentStyle

	class ParsingError : public std::runtime_error
	{
	  public:
		ParsingError(const QString& what);
	};

	class Parser
	{
	  public:
		Parser(FlagStyle::Enum flagStyle = FlagStyle::Default,
			   ArgumentStyle::Enum argStyle = ArgumentStyle::Default);

		void setFlagStyle(FlagStyle::Enum style);

		FlagStyle::Enum flagStyle();

		void setArgumentStyle(ArgumentStyle::Enum style);

		ArgumentStyle::Enum argumentStyle();

		void addSwitch(QString name, bool def = false);

		void addOption(QString name, QVariant def = QVariant());

		void addArgument(QString name, bool required = true,
						 QVariant def = QVariant());

		/** Adds a flag character to an existing parameter; a parameter can have
		 * only one flag. */
		void addShortOpt(QString name, QChar flag);

		/** Adds documentation to a parameter. @p metavar is the placeholder
		 * shown for the value (for positional arguments it replaces the name). */
		void addDocumentation(QString name, QString doc,
							  QString metavar = QString());

		QString compileHelp(QString progName, int helpIndent = 22,
							bool flagsInUsage = true);

		QString compileUsage(QString progName, bool useFlags = true);

		QHash<QString, QVariant> parse(QStringList argv);

		void clear();

		~Parser();

	  private:
		FlagStyle::Enum m_flagStyle;
		ArgumentStyle::Enum m_argStyle;

		enum OptionType { otSwitch, otOption };

		// Important: the common part MUST BE COMMON ON ALL THREE structs
		struct CommonDef {
			QString name;
			QString doc;
			QString metavar;
			QVariant def;
		};

		struct OptionDef {
			QString name;
			QString doc;
			QString metavar;
			QVariant def;
			OptionType type;
			QChar flag;
		};

		struct PositionalDef {
			QString name;
			QString doc;
			QString metavar;
			QVariant def;
			bool required;
		};

		QHash<QString, OptionDef*> m_options;
		QHash<QChar, OptionDef*> m_flags;
		QHash<QString, CommonDef*> m_params;
		QList<PositionalDef*> m_positionals;
		QList<OptionDef*> m_optionList;

		void getPrefix(QString& opt, QString& flag);
	};
} // namespace Commandline
