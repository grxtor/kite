/* SPDX-FileCopyrightText: 2026 grxtor
 * SPDX-FileContributor: grxtor
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (C) 2026 grxtor
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

#include <QImage>

/*
 * The one place a skin texture becomes a head. AccountFaceProvider (the
 * launcher's accounts) and FriendsFaces (a friend's cached head) both
 * draw it from here. A pure function of the image: no account, no network.
 */
namespace SkinFace
{
	/// The face (the 8x8 region at (8,8)) with the hat overlay (the 8x8
	/// region at (40,8)) drawn over it, at native 8x8. Hat pixels with alpha
	/// 0 leave the face beneath untouched. Null if @p skin is too small to
	/// hold both regions.
	QImage fromSkin(const QImage& skin);
} // namespace SkinFace
