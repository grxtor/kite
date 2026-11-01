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

#include <memory>
#include <QString>
#include <QMetaType>

class BaseVersion
{
  public:
	virtual ~BaseVersion() {}
	/* A string identifying this version in config files; unique within the
	 * version list. */
	virtual QString descriptor() = 0;

	/* The name shown to the user, e.g. "1.5.1". */
	virtual QString name() = 0;

	/* The kind of version this is (Stable, Beta, Snapshot, ...). */
	virtual QString typeString() const = 0;

	virtual bool operator<(BaseVersion& a)
	{
		return name() < a.name();
	};
	virtual bool operator>(BaseVersion& a)
	{
		return name() > a.name();
	};
};

typedef std::shared_ptr<BaseVersion> BaseVersionPtr;

Q_DECLARE_METATYPE(BaseVersionPtr)
