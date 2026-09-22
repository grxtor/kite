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

#include "core/AuthRequestDecorator.h"

class PluginManager;

/*
 * Feeds outgoing authentication requests through MMCO_HOOK_AUTH_REQUEST.
 *
 * This used to live inside AuthRequest.cpp, which meant the authentication
 * code reached PluginManager directly -- and PluginManager builds
 * plugin-supplied user interface, so that single call pulled QtWidgets into
 * the core. The hook logic is unchanged; only its address moved.
 */
class PluginAuthRequestDecorator final : public AuthRequestDecorator
{
  public:
	explicit PluginAuthRequestDecorator(PluginManager* manager);

	bool dispatchAuthRequest(QNetworkRequest& request, const QByteArray& body,
							 const char* method) override;

  private:
	PluginManager* m_manager;
};
