/* SPDX-FileCopyrightText: 2026 grxtor
 * SPDX-License-Identifier: Apache-2.0
 */

#include <QDir>
#include <QGuiApplication>
#include <QIcon>
#include <QPixmap>
#include <QSet>
#include <QTest>

#include "icons/AutoIcons.h"

namespace
{
	const QStringList kInstanceIconDirs = {
		QStringLiteral(":/icons/multimc/32x32/instances/"),
		QStringLiteral(":/icons/multimc/50x50/instances/"),
		QStringLiteral(":/icons/multimc/128x128/instances/"),
		QStringLiteral(":/icons/multimc/scalable/instances/"),
	};
	constexpr int kLargestAskedPx = 256;
} // namespace

/* Every icon AutoIcons hands out is a built-in instance icon (one left out of
 * multimc.qrc would quietly draw as the grass block), and the theme has it at
 * 256 px: rows ask for twice their size and a 2x screen doubles that. */
class AutoIconsBuiltInTest : public QObject
{
	Q_OBJECT

  private slots:
	void autoIconsAreBuiltIn()
	{
		QSet<QString> builtIn;
		for (const QString& dirPath : kInstanceIconDirs) {
			const QFileInfoList entries =
				QDir(dirPath).entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
			for (const QFileInfo& entry : entries) {
				builtIn.insert(entry.baseName());
			}
		}
		QVERIFY(!builtIn.isEmpty());

		QStringList searchPaths = QIcon::themeSearchPaths();
		searchPaths.prepend(QStringLiteral(":/icons"));
		QIcon::setThemeSearchPaths(searchPaths);
		QIcon::setThemeName(QStringLiteral("multimc"));

		const QStringList pool = AutoIcons::pool();
		QVERIFY(!pool.isEmpty());
		const QSize largest(kLargestAskedPx, kLargestAskedPx);
		for (const QString& key : pool) {
			QVERIFY2(builtIn.contains(key), qPrintable(key));
			const QPixmap big = QIcon::fromTheme(key).pixmap(largest, 1.0);
			QVERIFY2(big.size() == largest, qPrintable(key));
		}
	}
};

int main(int argc, char* argv[])
{
	qputenv("QT_QPA_PLATFORM", "offscreen");
	QGuiApplication app(argc, argv);

	/* "multimc" sits in the Kite_logic static library; without a symbol
	 * reference the linker drops the object that registers it. */
	Q_INIT_RESOURCE(multimc);

	AutoIconsBuiltInTest test;
	return QTest::qExec(&test, argc, argv);
}

#include "AutoIconsBuiltIn_test.moc"
