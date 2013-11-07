/**********************************************************************
 * LeechCraft - modular cross-platform feature rich internet client.
 * Copyright (C) 2006-2013  Georg Rudoy
 *
 * Boost Software License - Version 1.0 - August 17th, 2003
 *
 * Permission is hereby granted, free of charge, to any person or organization
 * obtaining a copy of the software and accompanying documentation covered by
 * this license (the "Software") to use, reproduce, display, distribute,
 * execute, and transmit the Software, and to prepare derivative works of the
 * Software, and to permit third-parties to whom the Software is furnished to
 * do so, all subject to the following:
 *
 * The copyright notices in the Software and this entire statement, including
 * the above license grant, this restriction and the following disclaimer,
 * must be included in all copies of the Software, in whole or in part, and
 * all derivative works of the Software, unless such copies or derivative
 * works are solely in the form of machine-executable object code generated by
 * a source language processor.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
 * SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
 * FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 **********************************************************************/

#include "requesthandler.h"
#include <sys/sendfile.h>
#include <errno.h>
#include <QList>
#include <QString>
#include <QtDebug>
#include <QFileInfo>
#include <QDir>
#include <QDateTime>
#include <util/util.h>
#include <util/sys/mimedetector.h>
#include "connection.h"
#include "storagemanager.h"
#include "iconresolver.h"
#include "trmanager.h"

namespace LeechCraft
{
namespace HttHare
{
	RequestHandler::RequestHandler (const Connection_ptr& conn)
	: Conn_ (conn)
	{
		ResponseHeaders_.append ({ "Accept-Ranges", "bytes" });
	}

	void RequestHandler::operator() (QByteArray data)
	{
		data.replace ("\r", "");

		auto lines = data.split ('\n');
		for (auto& line : lines)
			line = line.trimmed ();
		lines.removeAll ({});

		if (lines.size () <= 0)
			return ErrorResponse (400, "Bad Request");

		const auto& req = lines.takeAt (0).split (' ');
		if (req.size () < 2)
			return ErrorResponse (400, "Bad Request");

		const auto& verb = req.at (0).toLower ();
		Url_ = QUrl::fromEncoded (req.at (1));

		for (const auto& line : lines)
		{
			const auto colonPos = line.indexOf (':');
			if (colonPos <= 0)
				return ErrorResponse (400, "Bad Request");
			Headers_ [line.left (colonPos)] = line.mid (colonPos + 1).trimmed ();
		}

#ifdef QT_DEBUG
		qDebug () << Q_FUNC_INFO << "got request";
		qDebug () << req << Url_;
		for (auto i = Headers_.begin (); i != Headers_.end (); ++i)
			qDebug () << '\t' << i.key () << ": " << i.value ();
#endif

		if (verb == "head")
			HandleRequest (Verb::Head);
		else if (verb == "get")
			HandleRequest (Verb::Get);
		else
			return ErrorResponse (405, "Method Not Allowed",
					"Method " + verb + " not supported by this server.");
	}

	QString RequestHandler::Tr (const char *msg)
	{
		auto locales = Headers_ ["Accept-Language"].split (',');
		locales.removeAll ("*");
		for (auto& locale : locales)
		{
			const auto cpPos = locale.indexOf (';');
			if (cpPos >= 0)
				locale = locale.left (cpPos);
			locale = locale.trimmed ();
		}
		if (!locales.contains ("en"))
			locales << "en";

		auto mgr = Conn_->GetTrManager ();
		return mgr->Translate (locales, "LeechCraft::HttHare::RequestHandler", msg);
	}

	void RequestHandler::ErrorResponse (int code,
			const QByteArray& reason, const QByteArray& full)
	{
		ResponseLine_ = "HTTP/1.1 " + QByteArray::number (code) + " " + reason + "\r\n";

		ResponseBody_ = QString (R"delim(<html>
				<head><title>%1 %2</title></head>
				<body>
					<h1>%1 %2</h1>
					%3
				</body>
			</html>
			)delim")
				.arg (code)
				.arg (reason.data ())
				.arg (full.data ()).toUtf8 ();

		DefaultWrite (Verb::Get);
	}

	namespace
	{
		QString NormalizeClass (QString mime)
		{
			return mime.replace ('/', '_')
					.replace ('-', '_')
					.replace ('.', '_')
					.replace ('+', '_');
		}

		const auto IconSize = 16;
	}

	QByteArray RequestHandler::MakeDirResponse (const QFileInfo& fi, const QString& path, const QUrl& url)
	{
		const auto& entries = QDir { path }
				.entryInfoList (QDir::AllEntries | QDir::NoDotAndDotDot,
						QDir::Name | QDir::DirsFirst);

		struct MimeInfo
		{
			QString MimeType_;
		};
		QHash<QString, QByteArray> mimeCache;
		QList<MimeInfo> mimes;
		Util::MimeDetector detector;
		for (const auto& entry : entries)
		{
			const auto& type = detector (entry.filePath ());

			if (!mimeCache.contains (type))
			{
				QByteArray image;
				QMetaObject::invokeMethod (Conn_->GetIconResolver (),
						"resolveMime",
						Qt::BlockingQueuedConnection,
						Q_ARG (QString, type),
						Q_ARG (QByteArray&, image),
						Q_ARG (int, IconSize));
				mimeCache [type] = image;
			}

			mimes.append ({ type });
		}

		QString result;
		result += "<html><head><title>" + fi.fileName () + "</title><style>";
		for (auto pos = mimeCache.begin (); pos != mimeCache.end (); ++pos)
		{
			result += "." + NormalizeClass (pos.key ()) + " {";
			result += "background-image: url('" + pos.value () + "');";
			result += "background-repeat: no-repeat;";
			result += "padding-left: " + QString::number (IconSize + 4) + ";";
			result += "}";
		}
		result += "</style></head><body><h1>" + Tr ("Listing of %1").arg (url.toString ()) + "</h1>";
		result += "<table style='width: 100%'><tr>";
		result += QString ("<th style='width: 60%'>%1</th><th style='width: 20%'>%2</th><th style='width: 20%'>%3</th>")
					.arg (Tr ("Name"))
					.arg (Tr ("Size"))
					.arg (Tr ("Created"));

		for (int i = 0; i < entries.size (); ++i)
		{
			const auto& item = entries.at (i);

			result += "<tr><td class=" + NormalizeClass (mimes.at (i).MimeType_) + "><a href='";
			result += item.fileName () + "'>" + item.fileName () + "</a></td>";
			result += "<td>" + Util::MakePrettySize (item.size ()) + "</td>";
			result += "<td>" + item.created ().toString () + "</td></tr>";
		}

		result += "</table></body></html>";

		return result.toUtf8 ();
	}

	namespace
	{
		QList<QPair<qint64, qint64>> ParseRanges (QString str, qint64 fullSize)
		{
			QList<QPair<qint64, qint64>> result;

			const auto eqPos = str.indexOf ('=');
			if (eqPos >= 0)
				str = str.mid (eqPos + 1);

			const auto pcPos = str.indexOf (';');
			if (pcPos >= 0)
				str = str.left (pcPos);

			for (const auto& elem : str.split (',', QString::SkipEmptyParts))
			{
				const auto dashPos = elem.indexOf ('-');

				if (dashPos < 0)
					continue;

				const auto& startStr = elem.left (dashPos);
				const auto& endStr = elem.mid (dashPos + 1);
				if (startStr.isEmpty ())
				{
					bool ok = false;
					const auto last = endStr.toULongLong (&ok);
					if (!ok)
						continue;

					if (last)
						result.append ({ fullSize - last - 1, fullSize - 1 });
				}
				else
				{
					bool ok = false;
					const auto last = endStr.isEmpty () ?
							(ok = true, fullSize - 1) :
							endStr.toULongLong (&ok);
					if (!ok)
						continue;

					ok = false;
					const auto first = startStr.toULongLong (&ok);
					if (!ok)
						continue;

					if (first <= last)
						result.append ({ first, last });
				}
			}

			for (const auto& range : result)
				if (!range.first && range.second == fullSize - 1)
					return {};

			return result;
		}
	}

	namespace
	{
		struct Sendfiler
		{
			boost::asio::ip::tcp::socket& Sock_;
			std::shared_ptr<QFile> File_;
			off_t Offset_;

			QPair<qint64, qint64> CurrentRange_;
			QList<QPair<qint64, qint64>> TailRanges_;

			std::function<void (boost::system::error_code, ulong)> Handler_;

			void operator() (boost::system::error_code ec, ulong)
			{
				for (qint64 toTransfer = CurrentRange_.second - CurrentRange_.first + 1; toTransfer > 0; )
				{
					off_t offset = CurrentRange_.first;
					const auto rc = sendfile (Sock_.native_handle (),
							File_->handle (), &offset, toTransfer);
					ec = boost::system::error_code (rc < 0 ? errno : 0,
							boost::asio::error::get_system_category ());

					if (rc > 0)
					{
						CurrentRange_.first = offset;
						toTransfer -= rc;
					}

					if (ec == boost::asio::error::interrupted)
						continue;

					if (ec == boost::asio::error::would_block ||
							ec == boost::asio::error::try_again)
					{
						Sock_.async_write_some (boost::asio::null_buffers {}, *this);
						return;
					}

					if (ec)
						break;

					if (!toTransfer && !TailRanges_.isEmpty ())
					{
						CurrentRange_ = TailRanges_.takeFirst ();
						Sock_.async_write_some (boost::asio::null_buffers {}, *this);
						return;
					}
				}

				Handler_ (ec, 0);
			}
		};
	}

	void RequestHandler::HandleRequest (Verb verb)
	{
		QString path;
		try
		{
			path = Conn_->GetStorageManager ().ResolvePath (Url_);
		}
		catch (const AccessDeniedException&)
		{
			ResponseLine_ = "HTTP/1.1 403 Forbidden\r\n";

			ResponseHeaders_.append ({ "Content-Type", "text/html; charset=utf-8" });
			ResponseBody_ = QString (R"delim(<html>
					<head><title>%2</title></head>
					<body>
						%1
					</body>
				</html>
				)delim")
					.arg (Tr ("Access forbidden. You could return "
								"to the <a href='/'>root</a> of this server.")
							.arg (path))
					.arg (QFileInfo { Url_.path () }.fileName ())
					.toUtf8 ();

			DefaultWrite (verb);

			return;
		}

		const QFileInfo fi { path };
		if (!fi.exists ())
		{
			ResponseLine_ = "HTTP/1.1 404 Not found\r\n";

			ResponseHeaders_.append ({ "Content-Type", "text/html; charset=utf-8" });
			ResponseBody_ = QString (R"delim(<html>
					<head><title>%1</title></head>
					<body>
						%2
					</body>
				</html>
				)delim")
					.arg (fi.fileName ())
					.arg (Tr ("%1 is not found on this server").arg (path))
					.toUtf8 ();

			DefaultWrite (verb);
		}
		else if (fi.isDir ())
		{
			if (Url_.path ().endsWith ('/'))
			{
				ResponseLine_ = "HTTP/1.1 200 OK\r\n";

				ResponseHeaders_.append ({ "Content-Type", "text/html; charset=utf-8" });
				ResponseBody_ = MakeDirResponse (fi, path, Url_);

				DefaultWrite (verb);
			}
			else
			{
				ResponseLine_ = "HTTP/1.1 301 Moved Permanently\r\n";

				auto url = Url_;
				url.setPath (url.path () + '/');
				ResponseHeaders_.append ({ "Location", url.toString ().toUtf8 () });
				ResponseBody_ = MakeDirResponse (fi, path, url);

				DefaultWrite (verb);
			}
		}
		else
		{
			auto ranges = ParseRanges (Headers_.value ("Range"), fi.size ());

			ResponseHeaders_.append ({ "Content-Type", "application/octet-stream" });

			if (ranges.isEmpty ())
			{
				ResponseLine_ = "HTTP/1.1 200 OK\r\n";
				ResponseHeaders_.append ({ "Content-Length", QByteArray::number (fi.size ()) });
			}
			else
			{
				ResponseLine_ = "HTTP/1.1 206 Partial content\r\n";

				qint64 totalSize = 0;
				for (const auto& range : ranges)
					totalSize += range.second - range.first + 1;

				ResponseHeaders_.append ({ "Content-Length", QByteArray::number (totalSize) });
			}

			auto c = Conn_;
			boost::asio::async_write (c->GetSocket (),
					ToBuffers (verb),
					c->GetStrand ().wrap ([c, path, verb, ranges] (boost::system::error_code ec, ulong) mutable -> void
						{
							if (ec)
								qWarning () << Q_FUNC_INFO
										<< ec.message ().c_str ();

							auto& s = c->GetSocket ();

							std::shared_ptr<void> shutdownGuard = std::shared_ptr<void> (nullptr,
									[&s, &ec] (void*)
									{
										s.shutdown (boost::asio::socket_base::shutdown_both, ec);
									});

							if (verb != Verb::Get)
								return;

							std::shared_ptr<QFile> file { new QFile { path } };
							file->open (QIODevice::ReadOnly);

							if (ranges.isEmpty ())
								ranges.append ({ 0, file->size () - 1 });

							if (!s.native_non_blocking ())
								s.native_non_blocking (true, ec);

							const auto& headRange = ranges.takeFirst ();
							Sendfiler
							{
								s,
								file,
								0,
								headRange,
								ranges,
								[c, shutdownGuard] (boost::system::error_code ec, ulong) {}
							} (ec, 0);
						}));
		}
	}

	void RequestHandler::DefaultWrite (Verb verb)
	{
		auto c = Conn_;
		boost::asio::async_write (c->GetSocket (),
				ToBuffers (verb),
				c->GetStrand ().wrap ([c] (const boost::system::error_code& ec, ulong)
					{
						if (ec)
							qWarning () << Q_FUNC_INFO
									<< ec.message ().c_str ();

						boost::system::error_code iec;
						c->GetSocket ().shutdown (boost::asio::socket_base::shutdown_both, iec);
					}));
	}

	namespace
	{
		boost::asio::const_buffer BA2Buffer (const QByteArray& ba)
		{
			return { ba.constData (), static_cast<size_t> (ba.size ()) };
		}
	}

	std::vector<boost::asio::const_buffer> RequestHandler::ToBuffers (Verb verb)
	{
		std::vector<boost::asio::const_buffer> result;

		if (std::find_if (ResponseHeaders_.begin (), ResponseHeaders_.end (),
				[] (decltype (ResponseHeaders_.at (0)) pair)
					{ return pair.first.toLower () == "content-length"; }) == ResponseHeaders_.end ())
			ResponseHeaders_.append ({ "Content-Length", QByteArray::number (ResponseBody_.size ()) });

		CookedRH_.clear ();
		for (const auto& pair : ResponseHeaders_)
			CookedRH_ += pair.first + ": " + pair.second + "\r\n";
		CookedRH_ += "\r\n";

#ifdef QT_DEBUG
		qDebug () << Q_FUNC_INFO;
		qDebug () << '\t' << ResponseLine_.left (ResponseLine_.size () - 2);
		for (const auto& pair : ResponseHeaders_)
			qDebug () << '\t' << (pair.first + ": " + pair.second);
#endif

		result.push_back (BA2Buffer (ResponseLine_));
		result.push_back (BA2Buffer (CookedRH_));

		if (verb == Verb::Get)
			result.push_back (BA2Buffer (ResponseBody_));

		return result;
	}
}
}
