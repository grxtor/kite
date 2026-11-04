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

#include <QDir>
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QtTest>

#include <memory>

#include "friends/FriendsChat.h"
#include "friends/FriendsChatFakeServer.h"

/*
 * What the friends chat's QML tests share: a real FriendsChat over
 * FriendsChatFake::Server, an engine that sees it as `fakeChat`, the window a
 * test loads, and reading what it shows. With KITE_CHAT_SHOTS=<dir> shot()
 * saves the window there as a PNG; KITE_CHAT_SHOT_FACES=<dir> holds the
 * <name>.png heads face() hands out.
 */
namespace FriendChatQml
{
	inline const QString kDinnerbone = QStringLiteral("33333333-3333-4333-8333-333333333333");
	/* The friends panel opened out, about as wide as it is by default. */
	constexpr int kPanelWidth = 360;
	constexpr int kPanelHeight = 640;

	/* @p hour:@p minute local time, @p daysAgo days back, as the server writes times. */
	inline QString localIso(int daysAgo, int hour, int minute)
	{
		return QDateTime(QDate::currentDate().addDays(-daysAgo), QTime(hour, minute))
			.toUTC()
			.toString(Qt::ISODateWithMs);
	}

	inline QString face(const QString& name)
	{
		const QString dir = qEnvironmentVariable("KITE_CHAT_SHOT_FACES");
		const QString file = QDir(dir).filePath(name + QStringLiteral(".png"));
		return !dir.isEmpty() && QFile::exists(file) ? QUrl::fromLocalFile(file).toString() : QString();
	}

	/* Every visible text under @p item, depth first. */
	inline void collectTexts(QQuickItem* item, QStringList& out)
	{
		if (!item || !item->isVisible()) {
			return;
		}
		const QVariant text = item->property("text");
		if (item->inherits("QQuickText") && !text.toString().isEmpty()) {
			out << text.toString();
		}
		for (QQuickItem* child : item->childItems()) {
			collectTexts(child, out);
		}
	}
} // namespace FriendChatQml

class FriendChatQmlHarness : public QObject
{
  protected:
	std::unique_ptr<QQmlEngine> m_engine;
	FriendsChatFake::Server m_server;
	std::unique_ptr<FriendsChat> m_chat;
	std::unique_ptr<QObject> m_root;

	QQuickWindow* window() const
	{
		return qobject_cast<QQuickWindow*>(m_root.get());
	}
	QObject* find(const char* name) const
	{
		return m_root->findChild<QObject*>(QLatin1String(name));
	}
	/* The dialogs' overlay is in the content item too. */
	QStringList texts() const
	{
		QStringList out;
		FriendChatQml::collectTexts(window()->contentItem(), out);
		return out;
	}
	bool showing(const QString& text) const
	{
		const QStringList all = texts();
		if (!all.contains(text) && qEnvironmentVariableIsSet("KITE_CHAT_DEBUG")) {
			qDebug() << "on screen:" << all;
		}
		return all.contains(text);
	}
	void shot(const QString& name)
	{
		const QString dir = qEnvironmentVariable("KITE_CHAT_SHOTS");
		if (dir.isEmpty()) {
			return;
		}
		/* A frame for the last change to reach the screen. */
		constexpr int kSettleMs = 200;
		QTest::qWait(kSettleMs);
		const QImage image = window()->grabWindow();
		QVERIFY(image.save(QDir(dir).filePath(name + QStringLiteral(".png"))));
	}

	/* @p qml (a Window) loaded and shown; @p extra are more context objects by name. */
	void load(const QByteArray& qml, const QVariantMap& extra = {})
	{
		m_root.reset();
		m_engine = std::make_unique<QQmlEngine>();
		m_engine->addImportPath(QStringLiteral("qrc:/qt/qml"));
		m_engine->rootContext()->setContextProperty(QStringLiteral("fakeChat"), m_chat.get());
		for (auto it = extra.cbegin(); it != extra.cend(); ++it) {
			m_engine->rootContext()->setContextProperty(it.key(), it.value());
		}
		QQmlComponent component(m_engine.get());
		component.setData(qml, QUrl());
		m_root.reset(component.create());
		QVERIFY2(m_root, qPrintable(component.errorString()));
		QVERIFY(QTest::qWaitForWindowExposed(window()));
	}

	/* A fresh FriendsChat and server; the style QmlShell::show() picks. */
	void setUp()
	{
		QQuickStyle::setStyle(QStringLiteral("Kite.Style"));
		m_server = FriendsChatFake::Server();
		m_chat = std::make_unique<FriendsChat>();
		m_chat->setTransport(m_server.transport());
		m_chat->setMe(FriendsChatFake::kMe);
	}

	void tearDown()
	{
		m_root.reset();
		m_engine.reset();
		m_chat.reset();
	}
};
