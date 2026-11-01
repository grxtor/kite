/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileContributor: Project Tick
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright 2013-2021 MultiMC Contributors
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

#include "NetAction.h"
#include "HttpMetaCache.h"
#include "Validator.h"
#include "Sink.h"

#include "QObjectPtr.h"

namespace Net
{
	/* Why a Modrinth file is being downloaded. Modrinth shows authors
	 * per-version download counts; this tells downloads made while installing
	 * or updating a modpack from direct installs. Sent only to Modrinth's CDN.
	 *
	 * An empty `reason` means send nothing; the other fields are context and
	 * are left out individually when unknown. */
	struct ModrinthDownloadMeta {
		QString reason;
		QString gameVersion;
		QString loader;
		/* The pack version this download is part of, if any. */
		QString dependentOn;

		bool isEmpty() const
		{
			return reason.isEmpty();
		}

		QByteArray toJson() const;
	};

	class Download : public NetAction
	{
		Q_OBJECT

	  public: /* types */
		typedef shared_qobject_ptr<class Download> Ptr;
		enum class Option { NoOptions = 0, AcceptLocalFiles = 1 };
		Q_DECLARE_FLAGS(Options, Option)

	  protected: /* con/des */
		explicit Download();

	  public:
		virtual ~Download() {};
		static Download::Ptr makeCached(QUrl url, MetaEntryPtr entry,
										Options options = Option::NoOptions);
		static Download::Ptr makeByteArray(QUrl url, QByteArray* output,
										   Options options = Option::NoOptions);
		static Download::Ptr makeFile(QUrl url, QString path,
									  Options options = Option::NoOptions);

	  public: /* methods */
		QString getTargetFilepath()
		{
			return m_target_path;
		}
		void addValidator(Validator* v);
		bool abort() override;
		bool canAbort() override;

		/* Attaches download attribution for hosts that accept it; ignored for
		 * others, so mixed batches can set it uniformly. */
		void setModrinthDownloadMeta(const ModrinthDownloadMeta& meta)
		{
			m_modrinthMeta = meta;
		}

	  private: /* methods */
		bool handleRedirect();

	  protected slots:
		void downloadProgress(qint64 bytesReceived, qint64 bytesTotal) override;
		void downloadError(QNetworkReply::NetworkError error) override;
		void sslErrors(const QList<QSslError>& errors);
		void downloadFinished() override;
		void downloadReadyRead() override;

	  public slots:
		void startImpl() override;

	  private: /* data */
		// FIXME: remove this, it has no business being here.
		QString m_target_path;
		std::unique_ptr<Sink> m_sink;
		Options m_options;
		ModrinthDownloadMeta m_modrinthMeta;
	};
} // namespace Net

Q_DECLARE_OPERATORS_FOR_FLAGS(Net::Download::Options)
