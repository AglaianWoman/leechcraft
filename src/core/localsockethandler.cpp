/**********************************************************************
 * LeechCraft - modular cross-platform feature rich internet client.
 * Copyright (C) 2006-2014  Georg Rudoy
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

#include "localsockethandler.h"
#include <cstdlib>
#include <vector>
#include <QDataStream>
#include <QLocalSocket>
#include <QUrl>
#include <QFile>
#include "util/xpc/util.h"
#include "interfaces/structures.h"
#include "application.h"

namespace LeechCraft
{
	LocalSocketHandler::LocalSocketHandler ()
	: Server_ (new QLocalServer)
	{
		if (!Server_->listen (Application::GetSocketName ()))
		{
			if (!static_cast<Application*> (qApp)->IsAlreadyRunning ())
			{
				qWarning () << Q_FUNC_INFO
					<< "WTF? We cannot listen() on the local server but aren't running";
				std::exit (Application::EGeneralSocketError);
			}
			else if (!qobject_cast<Application*> (qApp)->GetVarMap ().count ("plugin"))
				std::exit (Application::EAlreadyRunning);
		}
		connect (Server_.get (),
				SIGNAL (newConnection ()),
				this,
				SLOT (handleNewLocalServerConnection ()));
	}

	void LocalSocketHandler::handleNewLocalServerConnection ()
	{
		std::unique_ptr<QLocalSocket> socket (Server_->nextPendingConnection ());
		if (!socket->bytesAvailable ())
			socket->waitForReadyRead (2000);

		if (!socket->bytesAvailable ())
		{
			qWarning () << Q_FUNC_INFO
					<< "no data read from the socket";
			return;
		}

		QByteArray read = socket->readAll ();
		QDataStream in (read);
		QStringList arguments;
		in >> arguments;

		if (!arguments.isEmpty ())
			arguments.removeFirst ();

		qDebug () << Q_FUNC_INFO << arguments;

		std::vector<std::wstring> strings;
		for (const auto& arg : arguments)
			strings.push_back (arg.toStdWString ());

		boost::program_options::options_description desc;
		boost::program_options::wcommand_line_parser parser (strings);
		auto map = qobject_cast<Application*> (qApp)->Parse (parser, &desc);
		DoLine (map);
	}

	void LocalSocketHandler::pullCommandLine ()
	{
		DoLine (qobject_cast<Application*> (qApp)->GetVarMap ());
	}

	namespace
	{
		QVariantMap GetAdditionalMap (const boost::program_options::variables_map& map)
		{
			QVariantMap addMap;

			const auto pos = map.find ("additional");
			if (pos == map.end ())
				return addMap;

			for (const auto& add : pos->second.as<std::vector<std::string>> ())
			{
				const auto& str = QString::fromStdString (add);
				const auto& name = str.section (':', 0, 0);
				const auto& value = str.section (':', 1);
				if (value.isEmpty ())
				{
					qWarning () << Q_FUNC_INFO
							<< "malformed Additional parameter:"
							<< str;
					continue;
				}

				addMap [name] = value;
			}
			return addMap;
		}
	}

	void LocalSocketHandler::DoLine (const boost::program_options::variables_map& map)
	{
		if (!map.count ("entity"))
			return;

		TaskParameters tp { FromCommandLine };
		if (map.count ("automatic"))
			tp |= AutoAccept;
		else
			tp |= FromUserInitiated;

		if (map.count ("handle"))
		{
			tp |= OnlyHandle;
			tp |= AutoAccept;
		}

		if (map.count ("download"))
		{
			tp |= OnlyDownload;
			tp |= AutoAccept;
		}

		QString type;
		try
		{
			if (map.find ("type") != map.end ())
				type = QString::fromUtf8 (map ["type"].as<std::string> ().c_str ());
		}
		catch (const std::exception& e)
		{
			qWarning () << Q_FUNC_INFO
					<< e.what ();
		}

		const auto& addMap = GetAdditionalMap (map);

		for (const auto& rawEntity : map ["entity"].as<std::vector<std::wstring>> ())
		{
			QVariant ve;

			const auto& entity = QString::fromWCharArray (rawEntity.c_str ());

			if (type == "url")
				ve = QUrl (entity);
			else if (type == "url_encoded")
				ve = QUrl::fromEncoded (entity.toUtf8 ());
			else if (type == "file")
				ve = QUrl::fromLocalFile (entity);
			else
			{
				if (QFile::exists (entity))
					ve = QUrl::fromLocalFile (entity);
				else if (QUrl::fromEncoded (entity.toUtf8 ()).isValid ())
					ve = QUrl::fromEncoded (entity.toUtf8 ());
				else
					ve = entity;
			}

			auto e = Util::MakeEntity (ve,
					{},
					tp);
			e.Additional_ = addMap;
			qDebug () << e.Entity_ << e.Additional_;
			emit gotEntity (e);
		}
	}
}
