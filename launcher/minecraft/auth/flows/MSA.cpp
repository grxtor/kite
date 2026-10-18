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

#include "MSA.h"

#include "minecraft/auth/steps/MSAStep.h"
#include "minecraft/auth/steps/XboxUserStep.h"
#include "minecraft/auth/steps/XboxAuthorizationStep.h"
#include "minecraft/auth/steps/KiteLoginStep.h"
#include "minecraft/auth/steps/XboxProfileStep.h"
#include "minecraft/auth/steps/EntitlementsStep.h"
#include "minecraft/auth/steps/MinecraftProfileStep.h"
#include "minecraft/auth/steps/GetSkinStep.h"

MSASilent::MSASilent(AccountData* data, QObject* parent)
	: AuthFlow(data, parent)
{
	m_steps.append(new MSAStep(m_data, MSAStep::Action::Refresh));
	m_steps.append(new XboxUserStep(m_data));
	m_steps.append(new XboxAuthorizationStep(m_data, &m_data->xboxApiToken,
											 "http://xboxlive.com", "Xbox"));
	m_steps.append(
		new XboxAuthorizationStep(m_data, &m_data->mojangservicesToken,
								  "rp://api.minecraftservices.com/", "Mojang"));
	m_steps.append(new KiteLoginStep(m_data));
	m_steps.append(new XboxProfileStep(m_data));
	m_steps.append(new EntitlementsStep(m_data));
	m_steps.append(new MinecraftProfileStep(m_data));
	m_steps.append(new GetSkinStep(m_data));
}

MSAInteractive::MSAInteractive(AccountData* data, QObject* parent)
	: AuthFlow(data, parent)
{
	m_steps.append(new MSAStep(m_data, MSAStep::Action::Login));
	m_steps.append(new XboxUserStep(m_data));
	m_steps.append(new XboxAuthorizationStep(m_data, &m_data->xboxApiToken,
											 "http://xboxlive.com", "Xbox"));
	m_steps.append(
		new XboxAuthorizationStep(m_data, &m_data->mojangservicesToken,
								  "rp://api.minecraftservices.com/", "Mojang"));
	m_steps.append(new KiteLoginStep(m_data));
	m_steps.append(new XboxProfileStep(m_data));
	m_steps.append(new EntitlementsStep(m_data));
	m_steps.append(new MinecraftProfileStep(m_data));
	m_steps.append(new GetSkinStep(m_data));
}
