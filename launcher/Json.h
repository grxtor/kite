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

#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDateTime>
#include <QUrl>
#include <QDir>
#include <QUuid>
#include <QVariant>
#include <memory>
#include <optional>

#include "Exception.h"

namespace Json
{
	class JsonException : public ::Exception
	{
	  public:
		JsonException(const QString& message) : Exception(message) {}
	};

	/// @throw FileSystemException
	void write(const QJsonDocument& doc, const QString& filename);
	/// @throw FileSystemException
	void write(const QJsonObject& object, const QString& filename);
	/// @throw FileSystemException
	void write(const QJsonArray& array, const QString& filename);

	QByteArray toBinary(const QJsonObject& obj);
	QByteArray toBinary(const QJsonArray& array);
	QByteArray toText(const QJsonObject& obj);
	QByteArray toText(const QJsonArray& array);

	/// @throw JsonException
	QJsonDocument requireDocument(const QByteArray& data,
								  const QString& what = "Document");
	/// @throw JsonException
	QJsonDocument requireDocument(const QString& filename,
								  const QString& what = "Document");
	/// @throw JsonException
	QJsonObject requireObject(const QJsonDocument& doc,
							  const QString& what = "Document");
	/// @throw JsonException
	QJsonArray requireArray(const QJsonDocument& doc,
							const QString& what = "Document");

	/// Parses @p data without throwing: nullopt on a parse error. A non-null
	/// @p context logs "<context> <errorString>" as a warning; @p error, if
	/// given, receives the error string (empty on success).
	std::optional<QJsonDocument> tryParse(const QByteArray& data,
										  const char* context = nullptr,
										  QString* error = nullptr);
	/// As tryParse(), but the warning reads
	/// "<context> <offset> reason: <errorString>" (the "at" is the caller's).
	std::optional<QJsonDocument> tryParseWithOffset(const QByteArray& data,
													const char* context,
													QString* error = nullptr);

	void writeString(QJsonObject& to, const QString& key, const QString& value);
	void writeStringList(QJsonObject& to, const QString& key,
						 const QStringList& values);

	template <typename T> QJsonValue toJson(const T& t)
	{
		return QJsonValue(t);
	}
	template <> QJsonValue toJson<QUrl>(const QUrl& url);
	template <> QJsonValue toJson<QByteArray>(const QByteArray& data);
	template <> QJsonValue toJson<QDateTime>(const QDateTime& datetime);
	template <> QJsonValue toJson<QDir>(const QDir& dir);
	template <> QJsonValue toJson<QUuid>(const QUuid& uuid);
	template <> QJsonValue toJson<QVariant>(const QVariant& variant);

	/// @throw JsonException
	template <typename T>
	T requireIsType(const QJsonValue& value, const QString& what = "Value");

	/// @throw JsonException
	template <>
	double requireIsType<double>(const QJsonValue& value, const QString& what);
	/// @throw JsonException
	template <>
	bool requireIsType<bool>(const QJsonValue& value, const QString& what);
	/// @throw JsonException
	template <>
	int requireIsType<int>(const QJsonValue& value, const QString& what);
	/// @throw JsonException
	template <>
	QJsonObject requireIsType<QJsonObject>(const QJsonValue& value,
										   const QString& what);
	/// @throw JsonException
	template <>
	QJsonArray requireIsType<QJsonArray>(const QJsonValue& value,
										 const QString& what);
	/// @throw JsonException
	template <>
	QJsonValue requireIsType<QJsonValue>(const QJsonValue& value,
										 const QString& what);
	/// @throw JsonException
	template <>
	QByteArray requireIsType<QByteArray>(const QJsonValue& value,
										 const QString& what);
	/// @throw JsonException
	template <>
	QDateTime requireIsType<QDateTime>(const QJsonValue& value,
									   const QString& what);
	/// @throw JsonException
	template <>
	QVariant requireIsType<QVariant>(const QJsonValue& value,
									 const QString& what);
	/// @throw JsonException
	template <>
	QString requireIsType<QString>(const QJsonValue& value,
								   const QString& what);
	/// @throw JsonException
	template <>
	QUuid requireIsType<QUuid>(const QJsonValue& value, const QString& what);
	/// @throw JsonException
	template <>
	QDir requireIsType<QDir>(const QJsonValue& value, const QString& what);
	/// @throw JsonException
	template <>
	QUrl requireIsType<QUrl>(const QJsonValue& value, const QString& what);

	template <typename T>
	T ensureIsType(const QJsonValue& value, const T default_ = T(),
				   const QString& what = "Value")
	{
		if (value.isUndefined() || value.isNull()) {
			return default_;
		}
		try {
			return requireIsType<T>(value, what);
		} catch (const JsonException&) {
			return default_;
		}
	}

	/// @throw JsonException
	template <typename T>
	T requireIsType(const QJsonObject& parent, const QString& key,
					const QString& what = "__placeholder__")
	{
		const QString localWhat =
			QString(what).replace("__placeholder__", '\'' + key + '\'');
		if (!parent.contains(key)) {
			throw JsonException(localWhat + "s parent does not contain " +
								localWhat);
		}
		return requireIsType<T>(parent.value(key), localWhat);
	}

	template <typename T>
	T ensureIsType(const QJsonObject& parent, const QString& key,
				   const T default_ = T(),
				   const QString& what = "__placeholder__")
	{
		const QString localWhat =
			QString(what).replace("__placeholder__", '\'' + key + '\'');
		if (!parent.contains(key)) {
			return default_;
		}
		return ensureIsType<T>(parent.value(key), default_, localWhat);
	}

	template <typename T> QVector<T> requireIsArrayOf(const QJsonDocument& doc)
	{
		const QJsonArray array = requireArray(doc);
		QVector<T> out;
		for (const QJsonValue val : array) {
			out.append(requireIsType<T>(val, "Document"));
		}
		return out;
	}

	template <typename T>
	QVector<T> ensureIsArrayOf(const QJsonValue& value,
							   const QString& what = "Value")
	{
		const QJsonArray array =
			ensureIsType<QJsonArray>(value, QJsonArray(), what);
		QVector<T> out;
		for (const QJsonValue val : array) {
			out.append(requireIsType<T>(val, what));
		}
		return out;
	}

	template <typename T>
	QVector<T> ensureIsArrayOf(const QJsonValue& value,
							   const QVector<T> default_,
							   const QString& what = "Value")
	{
		if (value.isUndefined()) {
			return default_;
		}
		return ensureIsArrayOf<T>(value, what);
	}

	/// @throw JsonException
	template <typename T>
	QVector<T> requireIsArrayOf(const QJsonObject& parent, const QString& key,
								const QString& what = "__placeholder__")
	{
		const QString localWhat =
			QString(what).replace("__placeholder__", '\'' + key + '\'');
		if (!parent.contains(key)) {
			throw JsonException(localWhat + "s parent does not contain " +
								localWhat);
		}
		return ensureIsArrayOf<T>(parent.value(key), localWhat);
	}

	template <typename T>
	QVector<T> ensureIsArrayOf(const QJsonObject& parent, const QString& key,
							   const QVector<T>& default_ = QVector<T>(),
							   const QString& what = "__placeholder__")
	{
		const QString localWhat =
			QString(what).replace("__placeholder__", '\'' + key + '\'');
		if (!parent.contains(key)) {
			return default_;
		}
		return ensureIsArrayOf<T>(parent.value(key), default_, localWhat);
	}

#define JSON_HELPERFUNCTIONS(NAME, TYPE)                                       \
	inline TYPE require##NAME(const QJsonValue& value,                         \
							  const QString& what = "Value")                   \
	{                                                                          \
		return requireIsType<TYPE>(value, what);                               \
	}                                                                          \
	inline TYPE ensure##NAME(const QJsonValue& value,                          \
							 const TYPE default_ = TYPE(),                     \
							 const QString& what = "Value")                    \
	{                                                                          \
		return ensureIsType<TYPE>(value, default_, what);                      \
	}                                                                          \
	inline TYPE require##NAME(const QJsonObject& parent, const QString& key,   \
							  const QString& what = "__placeholder__")         \
	{                                                                          \
		return requireIsType<TYPE>(parent, key, what);                         \
	}                                                                          \
	inline TYPE ensure##NAME(const QJsonObject& parent, const QString& key,    \
							 const TYPE default_ = TYPE(),                     \
							 const QString& what = "__placeholder")            \
	{                                                                          \
		return ensureIsType<TYPE>(parent, key, default_, what);                \
	}

	JSON_HELPERFUNCTIONS(Array, QJsonArray)
	JSON_HELPERFUNCTIONS(Object, QJsonObject)
	JSON_HELPERFUNCTIONS(JsonValue, QJsonValue)
	JSON_HELPERFUNCTIONS(String, QString)
	JSON_HELPERFUNCTIONS(Boolean, bool)
	JSON_HELPERFUNCTIONS(Double, double)
	JSON_HELPERFUNCTIONS(Integer, int)
	JSON_HELPERFUNCTIONS(DateTime, QDateTime)
	JSON_HELPERFUNCTIONS(Url, QUrl)
	JSON_HELPERFUNCTIONS(ByteArray, QByteArray)
	JSON_HELPERFUNCTIONS(Dir, QDir)
	JSON_HELPERFUNCTIONS(Uuid, QUuid)
	JSON_HELPERFUNCTIONS(Variant, QVariant)

#undef JSON_HELPERFUNCTIONS

} // namespace Json
using JSONValidationError = Json::JsonException;
