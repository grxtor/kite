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
#include <QObject>

#include "QObjectPtr.h"
#include "minecraft/auth/AuthStep.h"

class KiteLoginStep : public AuthStep
{
	Q_OBJECT

  public:
	explicit KiteLoginStep(AccountData* data);
	virtual ~KiteLoginStep() noexcept;

	void perform() override;
	void rehydrate() override;

	QString describe() override;

  private slots:
	void onRequestDone(QNetworkReply::NetworkError, QByteArray,
					   QList<QNetworkReply::RawHeaderPair>);
};
