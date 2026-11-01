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

#include <functional>
#include <memory>
#include <QObject>

namespace details
{
	struct DeleteQObjectLater {
		void operator()(QObject* obj) const
		{
			obj->deleteLater();
		}
	};
} // namespace details
/** A unique pointer for QObject derivatives that calls deleteLater() instead of
 * destroying the object immediately. */
template <typename T>
using unique_qobject_ptr = std::unique_ptr<T, details::DeleteQObjectLater>;

/** A shared pointer for QObject derivatives that calls deleteLater() instead of
 * destroying the object immediately. */
template <typename T> class shared_qobject_ptr
{
  public:
	shared_qobject_ptr() {}
	shared_qobject_ptr(T* wrap)
	{
		reset(wrap);
	}
	shared_qobject_ptr(const shared_qobject_ptr<T>& other)
	{
		m_ptr = other.m_ptr;
	}
	template <typename Derived>
	shared_qobject_ptr(const shared_qobject_ptr<Derived>& other)
		: m_ptr(other.unwrap())
	{
	}

  public:
	void reset(T* wrap)
	{
		if (wrap) {
			using namespace std::placeholders;
			m_ptr.reset(wrap, std::bind(&QObject::deleteLater, _1));
		} else {
			m_ptr.reset();
		}
	}
	void reset(const shared_qobject_ptr<T>& other)
	{
		m_ptr = other.m_ptr;
	}
	void reset()
	{
		m_ptr.reset();
	}
	T* get() const
	{
		return m_ptr.get();
	}
	T* operator->() const
	{
		return m_ptr.get();
	}
	T& operator*() const
	{
		return *m_ptr.get();
	}
	operator bool() const
	{
		return m_ptr.get() != nullptr;
	}
	const std::shared_ptr<T> unwrap() const
	{
		return m_ptr;
	}
	bool operator==(const shared_qobject_ptr<T>& other) const
	{
		return m_ptr == other.m_ptr;
	}
	bool operator!=(const shared_qobject_ptr<T>& other) const
	{
		return m_ptr != other.m_ptr;
	}

  private:
	std::shared_ptr<T> m_ptr;
};
