/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileContributor: Project Tick
 * SPDX-FileContributor: grxtor
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

/* Small Qt helpers shared by call sites. */

#include <QtGlobal>
#include <QMouseEvent>
#include <QObject>
#include <QPointF>

#include <utility>

namespace QtCompat
{
	/* Connect a signal to a functor that fires at most once. */
	template <typename Sender, typename Signal, typename Receiver,
			  typename Functor>
	inline void connectOnce(Sender* sender, Signal signal, Receiver* receiver,
							Functor functor)
	{
		QObject::connect(sender, signal, receiver, std::move(functor),
						 Qt::SingleShotConnection);
	}

	/* Widget-local position of a mouse event. */
	inline QPointF mousePosition(const QMouseEvent* event)
	{
		return event->position();
	}
} // namespace QtCompat
