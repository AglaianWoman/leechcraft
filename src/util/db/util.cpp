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

#include "util.h"
#include <QFile>
#include <QSqlQuery>
#include <QThread>
#include "dblock.h"

namespace LeechCraft
{
namespace Util
{
	QSqlQuery RunTextQuery (const QSqlDatabase& db, const QString& text)
	{
		QSqlQuery query { db };
		query.prepare (text);

		DBLock::Execute (query);

		return query;
	}

	QString LoadQuery (const QString& pluginName, const QString& filename)
	{
		QFile file { ":/" + pluginName + "/resources/sql/" + filename + ".sql" };
		if (!file.open (QIODevice::ReadOnly))
		{
			qWarning () << Q_FUNC_INFO
					<< file.fileName ()
					<< file.errorString ();
			throw std::runtime_error { "Cannot open query file" };
		}

		return QString::fromUtf8 (file.readAll ());
	}

	void RunQuery (const QSqlDatabase& db, const QString& pluginName, const QString& filename)
	{
		QSqlQuery query { db };
		query.prepare (LoadQuery (pluginName, filename));
		Util::DBLock::Execute (query);
	}

	namespace
	{
		template<typename To, typename From>
		typename std::enable_if<std::is_same<From, To>::value, To>::type DumbCast (From from)
		{
			return from;
		}

		template<typename To, typename From>
		typename std::enable_if<!std::is_same<From, To>::value &&
					std::is_integral<From>::value &&
					std::is_integral<To>::value, To>::type DumbCast (From from)
		{
			return static_cast<To> (from);
		}

		template<typename To, typename From>
		typename std::enable_if<!std::is_same<From, To>::value &&
					!(std::is_integral<From>::value &&
						std::is_integral<To>::value), To>::type DumbCast (From from)
		{
			return reinterpret_cast<To> (from);
		}

		uintptr_t Handle2Num (Qt::HANDLE handle)
		{
			return DumbCast<uintptr_t> (handle);
		}
	}

	QString GenConnectionName (const QString& base)
	{
		return (base + ".%1_%2")
				.arg (qrand ())
				.arg (Handle2Num (QThread::currentThreadId ()));
	}
}
}
