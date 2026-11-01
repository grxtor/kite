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

#include <cstddef>
#include <memory>

#include "QObjectPtr.h"

class Usable;

/** Base class for things used by multiple other things whose use count we
 * track. @see UseLock */
class Usable
{
	friend class UseLock;

  public:
	bool isInUse() const
	{
		return m_useCount > 0;
	}

  protected:
	virtual void decrementUses()
	{
		m_useCount--;
	}
	virtual void incrementUses()
	{
		m_useCount++;
	}

  private:
	std::size_t m_useCount = 0;
};

/** Lock for keeping track of uses of a Usable. @see Usable */
class UseLock
{
  public:
	UseLock(shared_qobject_ptr<Usable> usable) : m_usable(usable)
	{
		// this doesn't use shared pointer use count, because that wouldn't be
		// correct. this count is separate.
		m_usable->incrementUses();
	}
	~UseLock()
	{
		m_usable->decrementUses();
	}

  private:
	shared_qobject_ptr<Usable> m_usable;
};
