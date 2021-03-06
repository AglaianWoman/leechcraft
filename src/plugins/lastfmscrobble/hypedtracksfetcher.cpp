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

#include "hypedtracksfetcher.h"
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QNetworkAccessManager>
#include <QDomDocument>
#include <QtDebug>
#include <util/network/handlenetworkreply.h>
#include <util/threads/futures.h>
#include "util.h"

namespace LeechCraft
{
namespace Lastfmscrobble
{
	HypedTracksFetcher::HypedTracksFetcher (QNetworkAccessManager *nam, Media::IHypesProvider::HypeType type, QObject *parent)
	: QObject (parent)
	{
		Promise_.reportStarted ();

		QMap<QString, QString> params;
		params ["limit"] = "50";
		const auto& method = type == Media::IHypesProvider::HypeType::NewTracks ?
				"chart.getHypedTracks" :
				"chart.getTopTracks";
		auto reply = Request (method, nam, params);
		Util::HandleReplySeq (reply, this) >>
				Util::Visitor
				{
					[this] (Util::Void) { Util::ReportFutureResult (Promise_, QString { "Unable to issue Last.FM API request." }); },
					[this] (const QByteArray& data) { HandleFinished (data); }
				}.Finally ([this] { deleteLater (); });
	}

	QFuture<Media::IHypesProvider::HypeQueryResult_t> HypedTracksFetcher::GetFuture ()
	{
		return Promise_.future ();
	}

	void HypedTracksFetcher::HandleFinished (const QByteArray& data)
	{
		QDomDocument doc;
		if (!doc.setContent (data))
		{
			qWarning () << Q_FUNC_INFO
					<< "error parsing reply"
					<< data;
			Util::ReportFutureResult (Promise_, QString { "Unable to parse Last.FM response." });
			return;
		}

		QList<Media::HypedTrackInfo> tracks;

		auto trackElem = doc
				.documentElement ()
				.firstChildElement ("tracks")
				.firstChildElement ("track");
		while (!trackElem.isNull ())
		{
			auto getText = [&trackElem] (const QString& name)
			{
				return trackElem.firstChildElement (name).text ();
			};

			const auto& artistElem = trackElem.firstChildElement ("artist");

			tracks << Media::HypedTrackInfo
			{
				getText ("name"),
				getText ("url"),
				getText ("percentagechange").toInt (),
				getText ("playcount").toInt (),
				getText ("listeners").toInt (),
				getText ("duration").toInt (),
				GetImage (trackElem, "medium"),
				GetImage (trackElem, "extralarge"),
				artistElem.firstChildElement ("name").text (),
				artistElem.firstChildElement ("url").text ()
			};

			trackElem = trackElem.nextSiblingElement ("track");
		}

		Util::ReportFutureResult (Promise_, tracks);
	}
}
}
