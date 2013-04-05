/**********************************************************************
 * LeechCraft - modular cross-platform feature rich internet client.
 * Copyright (C) 2006-2013  Georg Rudoy
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 **********************************************************************/

#pragma once

#include <QNetworkReply>
#include "utilconfig.h"

namespace LeechCraft
{
namespace Util
{
	/** @brief A network reply with customizable content and reply headers.
	 *
	 * This class provides a custom network reply with settable content
	 * and headers. It can be used, for example, in a plugin that renders
	 * local filesystem to QNetworkAccessManager-enabled plugins, or that
	 * just needs to provide a network reply with a predefined or
	 * runtime-generated string.
	 */
	class UTIL_API CustomNetworkReply : public QNetworkReply
	{
		Q_OBJECT

		QByteArray Content_;
		qint64 Offset_;
	public:
		/** @brief Creates the reply with the given url and parent.
		 *
		 * Sets the URL of this reply to be \em url. This URL will be
		 * returned by <code>QNetworkReply::url()</code>
		 *
		 * @param[in] url The URL this custom reply corresponds to.
		 * @param[in] parent The parent object of this object.
		 */
		CustomNetworkReply (const QUrl& url, QObject *parent = 0);

		/** @brief Virtual destructor.
		 */
		virtual ~CustomNetworkReply ();

		/** @brief Sets the network error of this reply.
		 *
		 * This function can be used to set the given network error with
		 * an optional reason string.
		 *
		 * @param[in] error The network error.
		 * @param[in] reason The additional reason string.
		 */
		void SetError (NetworkError error, const QString& reason = QString ());

		/** @brief Sets the given header to the given value.
		 *
		 * This function sets the given \em header to the given \em value.
		 *
		 * @param[in] header The known standard header to set.
		 * @param[in] value The value of the header.
		 *
		 * @sa SetContentType()
		 */
		void SetHeader (QNetworkRequest::KnownHeaders header, const QVariant& value);

		/** @brief Sets the content type of this reply.
		 *
		 * This function sets the <em>Content-Type</em> header to \em type.
		 *
		 * It is equivalent to
		 * \code
		 * SetHeader (QNetworkRequest::ContentType, type);
		 * \endcode
		 */
		void SetContentType (const QByteArray& type);

		/** @brief Sets content of this reply to the given string.
		 *
		 * This convenience function is equivalent to
		 * \code
		 * SetContent (string.toUtf8 ());
		 * \endcode
		 *
		 * @param[in] string The string to set.
		 */
		void SetContent (const QString& string);

		/** @brief Sets content of this reply to the given data.
		 *
		 * This function sets the content of this reply to the given
		 * \em data, updates the <em>Content-Length</em> header and
		 * schedules emission of the <code>readyRead()</code> and
		 * <code>finished()</code> signals next time control reaches the
		 * event loop.
		 *
		 * @param[in] data The data this network reply should contain.
		 */
		void SetContent (const QByteArray& data);

		/** @brief Reimplemented from QNetworkReply::abort().
		 *
		 * This function does nothing.
		 */
		void abort ();

		/** @brief Reimplemented from QNetworkReply::bytesAvailable().
		 *
		 * This function returns the number of bytes left unread.
		 */
		qint64 bytesAvailable () const;

		/** @brief Reimplemented from QNetworkReply::isSequential().
		 *
		 * This function always returns <code>true</code>.
		 */
		bool isSequential () const;
	protected:
		qint64 readData (char*, qint64);
	};
}
}
