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
#include <QList>
#include <QObject>
#include <QString>
#include <QVariantList>
#include <functional>
#include <memory>

#include "java/download/JavaRuntime.h"
#include "net/NetJob.h"

#ifndef MeshMC_DISABLE_JAVA_DOWNLOADER
class JavaDownloadTask;
#endif

/*
 * QML-facing "Download Java": the widget-free half of JavaDownloadDialog.
 * The user picks a vendor, then a Java version, then a build for this
 * computer; download() runs JavaDownloadTask into
 * <managed root>/<vendor>/<name-version> with progress.
 *
 * Every pick fetches the next list from the meta server. A pick made while
 * an earlier fetch is still running replaces it, so a slow answer for the
 * old pick can never overwrite the new one.
 *
 * Call reset() when the dialog opens: it forgets the last pick and loads
 * the first vendor's versions.
 */
class JavaDownloadController : public QObject
{
	Q_OBJECT

	/// [{value, label}], same as JavaController::vendors.
	Q_PROPERTY(QVariantList vendors READ vendors CONSTANT)
	Q_PROPERTY(QString vendor READ vendor NOTIFY vendorChanged)
	/// [{value, label, recommended}] of the chosen vendor.
	Q_PROPERTY(QVariantList versions READ versions NOTIFY versionsChanged)
	Q_PROPERTY(QString version READ version NOTIFY versionChanged)
	/// [{label, name, version, checksum}] of the chosen version that fit
	/// this computer.
	Q_PROPERTY(QVariantList builds READ builds NOTIFY buildsChanged)
	/// Row of the chosen build in builds(), or -1.
	Q_PROPERTY(int build READ build NOTIFY buildChanged)
	/// A list is being fetched.
	Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
	Q_PROPERTY(bool downloading READ downloading NOTIFY downloadingChanged)
	/// 0..1 while downloading.
	Q_PROPERTY(double progress READ progress NOTIFY progressChanged)
	/// What the download is doing right now, e.g. "Extracting".
	Q_PROPERTY(QString status READ status NOTIFY statusChanged)
	/// Empty unless the last fetch or download failed.
	Q_PROPERTY(QString error READ error NOTIFY errorChanged)
	/// A build is chosen and its folder already exists.
	Q_PROPERTY(bool alreadyInstalled READ alreadyInstalled NOTIFY
				   buildChanged)
	/// The java binary the last successful download installed.
	Q_PROPERTY(QString installedPath READ installedPath NOTIFY installed)

  public:
	explicit JavaDownloadController(QObject* parent = nullptr);
	~JavaDownloadController() override;

	QVariantList vendors() const;
	QString vendor() const;
	QVariantList versions() const;
	QString version() const;
	QVariantList builds() const;
	int build() const;
	bool loading() const;
	bool downloading() const;
	double progress() const;
	QString status() const;
	QString error() const;
	bool alreadyInstalled() const;
	QString installedPath() const;

	/// Forgets every pick and loads the first vendor's versions.
	Q_INVOKABLE void reset();
	Q_INVOKABLE void selectVendor(const QString& uid);
	Q_INVOKABLE void selectVersion(const QString& versionId);
	Q_INVOKABLE void selectBuild(int row);
	/// Downloads the chosen build; an existing copy is replaced.
	Q_INVOKABLE void download();
	/// Stops a running fetch or download.
	Q_INVOKABLE void cancel();

	/// The version to preselect in @p versions (rows as versionsFromJson()
	/// returns them): the recommended one, else the first, else empty.
	static QString defaultVersion(const QVariantList& versions);

	/// "java21" -> "Java 21".
	static QString versionLabel(const QString& versionId);

	/// Rows of a <uid>/index.json document: {value, label, recommended}.
	/// An empty list and @p error set if the document is not JSON.
	static QVariantList versionsFromJson(const QByteArray& json,
										 const QString& uid, QString* error);

	/// The runtimes of a <uid>/<version>.json document that run on
	/// @p runtimeOS. @p error is set if the document is not JSON.
	static QList<JavaDownload::RuntimeEntry>
	runtimesFromJson(const QByteArray& json, const QString& runtimeOS,
					 QString* error);

	/// Where @p runtime is installed.
	static QString targetDirFor(const JavaDownload::RuntimeEntry& runtime);

  signals:
	void vendorChanged();
	void versionsChanged();
	void versionChanged();
	void buildsChanged();
	void buildChanged();
	void loadingChanged();
	void downloadingChanged();
	void progressChanged();
	void statusChanged();
	void errorChanged();
	/// The download finished; the runtime is on disk.
	void installed();

  private:
	void fetch(const QString& relativeUrl,
			   std::function<void(const QByteArray&)> onData);
	void setLoading(bool loading);
	void setError(const QString& error);
	void stopFetch();

	QVariantList m_vendors;
	QString m_vendor;
	QList<JavaDownload::JavaVersionInfo> m_versionInfos;
	QVariantList m_versions;
	QString m_version;
	QList<JavaDownload::RuntimeEntry> m_runtimes;
	QVariantList m_builds;
	int m_build = -1;
	bool m_loading = false;
	bool m_downloading = false;
	double m_progress = 0;
	QString m_status;
	QString m_error;
	QString m_installedPath;

	NetJob::Ptr m_fetchJob;
	std::shared_ptr<QByteArray> m_fetchData;
#ifndef MeshMC_DISABLE_JAVA_DOWNLOADER
	std::unique_ptr<JavaDownloadTask> m_task;
#endif
};
