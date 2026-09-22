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

#include "plugin/PluginAuthRequestDecorator.h"

#include <QNetworkRequest>
#include <QUrl>

#include "plugin/PluginHooks.h"
#include "plugin/PluginManager.h"

PluginAuthRequestDecorator::PluginAuthRequestDecorator(PluginManager* manager)
	: m_manager(manager)
{
}

bool PluginAuthRequestDecorator::dispatchAuthRequest(QNetworkRequest& request,
													 const QByteArray& body,
													 const char* method)
{
	if (!m_manager)
		return false;

	/* The add_header callback closes over the request reference and
	 * appends raw headers. We keep it as a plain C function pointer with a
	 * sidecar state struct so the closure can survive the C ABI boundary. */
	struct HeaderCtx {
		QNetworkRequest* req;
	};
	HeaderCtx hctx{&request};

	auto add_header_fn = [](void* handle, const char* key,
							const char* value) -> int {
		if (!handle || !key || !value)
			return -1;
		auto* h = static_cast<HeaderCtx*>(handle);
		h->req->setRawHeader(QByteArray(key), QByteArray(value));
		return 0;
	};

	const QByteArray urlUtf8 = request.url().toString().toUtf8();

	MMCOAuthRequestEvent ev{};
	ev.url = urlUtf8.constData();
	ev.method = method;
	ev.body = body.isEmpty() ? nullptr : body.constData();
	ev.body_size = body.size();
	ev.redirect_url = nullptr;
	ev.request_handle = &hctx;
	ev.add_header = add_header_fn;

	const bool cancelled =
		m_manager->dispatchHook(MMCO_HOOK_AUTH_REQUEST, &ev);
	if (cancelled)
		return true;

	if (ev.redirect_url && *ev.redirect_url) {
		const QUrl rewritten =
			QUrl::fromUserInput(QString::fromUtf8(ev.redirect_url));
		if (rewritten.isValid())
			request.setUrl(rewritten);
	}
	return false;
}
