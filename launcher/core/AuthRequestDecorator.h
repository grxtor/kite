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

#include <QByteArray>

class QNetworkRequest;

/*
 * Lets something outside the core add headers to an outgoing authentication
 * request.
 *
 * In practice the implementation is the plugin host, which forwards to the
 * MMCO auth-request hook. AuthRequest needs that hook, but PluginManager
 * renders plugin-supplied user interface and so drags in QtWidgets; having the
 * authentication code call it directly would tie the core to the widget
 * toolkit through a single line. Hence this interface: the core declares what
 * it needs, the plugin layer supplies it, and neither knows about the other.
 */
class AuthRequestDecorator
{
  public:
	virtual ~AuthRequestDecorator() = default;

	/* Header and redirect changes are applied to `request` in place.
	 *
	 * Returns true when the request was CANCELLED -- the caller must then
	 * abort it and report a network error. It does NOT return true merely
	 * because the request was modified.
	 *
	 * `method` is the HTTP verb as an ASCII literal ("GET", "POST"); `body`
	 * is empty for verbs that carry none. */
	virtual bool dispatchAuthRequest(QNetworkRequest& request,
									 const QByteArray& body,
									 const char* method) = 0;
};
