/**********************************************************************
 * LeechCraft - modular cross-platform feature rich internet client.
 * Copyright (C) 2013  Slava Barinov <rayslava@gmail.com>
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

#include "twitterinterface.h"
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QDateTime>
#include <QDebug>
#include <qjson/parser.h>
#include <QtKOAuth/QtKOAuth>
#include "core.h"
#include "xmlsettingsmanager.h"

namespace LeechCraft
{
namespace Woodpecker
{
	TwitterInterface::TwitterInterface (QObject *parent)
	: QObject (parent)
	{
		HttpClient_ = Core::Instance ().GetCoreProxy ()->GetNetworkAccessManager ();
		OAuthRequest_ = new KQOAuthRequest (this);
		OAuthManager_ = new KQOAuthManager (this);

		OAuthRequest_->setEnableDebugOutput (false); // DONE: Remove debug
		ConsumerKey_ = XmlSettingsManager::Instance ()->property ("consumer_key").toString ();
		ConsumerKeySecret_ = XmlSettingsManager::Instance ()->property ("consumer_key_secret").toString ();

		connect (OAuthManager_,
				SIGNAL (requestReady (QByteArray)),
				this, 
				SLOT (onRequestReady (QByteArray)));

		connect (OAuthManager_,
				SIGNAL (authorizedRequestDone ()),
				this,
				SLOT (onAuthorizedRequestDone ()));
	}

	void TwitterInterface::RequestTwitter (const QUrl& requestAddress)
	{
		auto reply = HttpClient_->get (QNetworkRequest (requestAddress));
		connect (reply,
				SIGNAL (finished ()),
				this,
				SLOT (replyFinished ()));
		//           getAccess ();
		//           xauth ();
	}

	void TwitterInterface::replyFinished ()
	{
		QByteArray jsonText (qobject_cast<QNetworkReply*> (sender ())->readAll ());
		sender ()->deleteLater ();

		emit tweetsReady (ParseReply (jsonText));
	}

	QList<std::shared_ptr<Tweet>> TwitterInterface::ParseReply (const QByteArray& json)
	{
		QJson::Parser parser;
		QList<std::shared_ptr<Tweet>> result;
		bool ok;

		QVariantList answers = parser.parse (json, &ok).toList ();

		if (!ok) 
			qWarning () << "Parsing error at parseReply " << QString::fromUtf8 (json);

		QVariantMap tweetMap;
		QVariantMap userMap;

		for (int i = answers.count () - 1; i >= 0 ; --i)
		{
			tweetMap = answers [i].toMap ();
			userMap = tweetMap ["user"].toMap ();
			QLocale locale (QLocale::English);
			Tweet_ptr tempTweet (new Tweet ());

			tempTweet->SetText (tweetMap ["text"].toString ());
			tempTweet->GetAuthor ()->SetUsername (userMap ["screen_name"].toString ());
			tempTweet->GetAuthor ()->DownloadAvatar (userMap ["profile_image_url"].toString ());
			connect (tempTweet->GetAuthor ().get (),
					SIGNAL (userReady ()), 
					parent (),
					SLOT (setUpdateReady ()));
			tempTweet->SetDateTime (locale.toDateTime (tweetMap ["created_at"].toString (), QLatin1String ("ddd MMM dd HH:mm:ss +0000 yyyy")));
			tempTweet->SetId (tweetMap ["id"].toULongLong ());

			result.push_back (tempTweet);
		}

		return result;
	}

	void TwitterInterface::GetAccess () 
	{
		connect (OAuthManager_,
				SIGNAL (temporaryTokenReceived (QString, QString)),
				this,
				SLOT (onTemporaryTokenReceived (QString, QString)));

		connect (OAuthManager_,
				SIGNAL (authorizationReceived (QString, QString)),
				this, 
				SLOT (onAuthorizationReceived (QString, QString)));

		connect (OAuthManager_,
				SIGNAL (accessTokenReceived (QString, QString)),
				this,
				SLOT (onAccessTokenReceived (QString, QString)));

		OAuthRequest_->initRequest (KQOAuthRequest::TemporaryCredentials, QUrl ("https://api.twitter.com/oauth/request_token"));
		OAuthRequest_->setConsumerKey (ConsumerKey_);
		OAuthRequest_->setConsumerSecretKey (ConsumerKeySecret_);
		OAuthManager_->setHandleUserAuthorization (true);

		OAuthManager_->executeRequest (OAuthRequest_);

	}

	void TwitterInterface::SignedRequest (TwitterRequest req, KQOAuthRequest::RequestHttpMethod method, KQOAuthParameters params)
	{
		QUrl reqUrl;

		if (Token_.isEmpty () || TokenSecret_.isEmpty ())
		{
			qWarning () << "No access tokens. Aborting.";
			return;
		}

		switch (req)
		{
		case TwitterRequest::HomeTimeline:
			reqUrl = "https://api.twitter.com/1.1/statuses/home_timeline.json";
			params.insert ("count", "50");
			params.insert ("include_entities", "true");
			break;
			
		case TwitterRequest::UserTimeline:
			reqUrl = "http://api.twitter.com/1.1/statuses/user_timeline.json";
			params.insert ("include_entities", "true");
			break;
			
		case TwitterRequest::Update:
			reqUrl = "http://api.twitter.com/1.1/statuses/update.json";
			break;
			
		case TwitterRequest::Direct:
			reqUrl = "https://api.twitter.com/1.1/direct_messages.json";
			
		case TwitterRequest::Retweet:
			reqUrl = QString ("http://api.twitter.com/1.1/statuses/retweet/").append (params.value ("rt_id")).append (".json");
			break;
			
		case TwitterRequest::Reply:
			reqUrl = "http://api.twitter.com/1.1/statuses/update.json";
			break;
			
		case TwitterRequest::SpamReport:
			reqUrl = "http://api.twitter.com/1.1/report_spam.json";
			break;
			
		default:
			return;
		}
		
		OAuthRequest_->initRequest (KQOAuthRequest::AuthorizedRequest, reqUrl);
		OAuthRequest_->setHttpMethod (method);
		OAuthRequest_->setConsumerKey (ConsumerKey_);
		OAuthRequest_->setConsumerSecretKey (ConsumerKeySecret_);
		OAuthRequest_->setToken (Token_);
		OAuthRequest_->setTokenSecret (TokenSecret_);
		OAuthRequest_->setAdditionalParameters (params);
		OAuthManager_->executeRequest (OAuthRequest_);
	}
	
	void TwitterInterface::SendTweet (const QString& tweet)
	{
		KQOAuthParameters param;
		param.insert ("status", tweet);
		SignedRequest (TwitterRequest::Update, KQOAuthRequest::POST, param);
	}

	void TwitterInterface::Retweet (const qulonglong id)
	{
		KQOAuthParameters param;
		param.insert ("rt_id", QString::number (id));
		SignedRequest (TwitterRequest::Retweet, KQOAuthRequest::POST, param);
	}

	void TwitterInterface::Reply (const qulonglong replyid, const QString& tweet)
	{
		KQOAuthParameters param;
		param.insert ("status", tweet);
		param.insert ("in_reply_to_status_id", QString::number (replyid));
		SignedRequest (TwitterRequest::Reply, KQOAuthRequest::POST, param);
	}

	void TwitterInterface::onAuthorizedRequestDone ()
	{
		qDebug () << "Request sent to Twitter!";
	}

	void TwitterInterface::onRequestReady (const QByteArray& response)
	{
		qDebug () << "Response from the service: recvd";// << response;
		emit tweetsReady (ParseReply (response));
	}

	void TwitterInterface::onAuthorizationReceived (const QString& token, const QString& verifier)
	{
		OAuthManager_->getUserAccessTokens (QUrl ("https://api.twitter.com/oauth/access_token"));

		if (OAuthManager_->lastError () != KQOAuthManager::NoError) 
		{
			qWarning() << "Authorization error";
		}
	}

	void TwitterInterface::onAccessTokenReceived (const QString& token, const QString& tokenSecret) 
	{
		this->Token_ = token;
		this->TokenSecret_ = tokenSecret;

		qDebug () << "Access tokens now stored. You are ready to send Tweets from user's account!";

		emit authorized (token, tokenSecret);
	}

	void TwitterInterface::onTemporaryTokenReceived (const QString& token, const QString& tokenSecret)
	{
		QUrl userAuthURL ("https://api.twitter.com/oauth/authorize");

		if (OAuthManager_->lastError () == KQOAuthManager::NoError)
		{
			qDebug () << "Asking for user's permission to access protected resources. Opening URL: " << userAuthURL;
			OAuthManager_->getUserAuthorization (userAuthURL);
		}
	}

	void TwitterInterface::Xauth () 
	{
		connect (OAuthManager_, 
				SIGNAL (accessTokenReceived (QString, QString)),
				this, 
				SLOT (onAccessTokenReceived (QString, QString)));

		KQOAuthRequest_XAuth *oauthRequest = new KQOAuthRequest_XAuth (this);
		oauthRequest->initRequest (KQOAuthRequest::AccessToken, QUrl ("https://api.twitter.com/oauth/access_token"));
		oauthRequest->setConsumerKey ( XmlSettingsManager::Instance ()->property ("consumer_key").toString ());
		oauthRequest->setConsumerSecretKey (XmlSettingsManager::Instance ()->property ("consumer_key_secret").toString ());
		OAuthManager_->executeRequest (oauthRequest);
	}

	void TwitterInterface::searchTwitter (const QString& text)
	{
		QString link ("http://search.twitter.com/search.json?q=" + text);
		SetLastRequestMode (FeedMode::SearchResult);
		RequestTwitter (link);
	}

	void TwitterInterface::requestHomeFeed ()
	{
		qDebug () << "Getting home feed";
		SetLastRequestMode (FeedMode::HomeTimeline);
		SignedRequest (TwitterRequest::HomeTimeline, KQOAuthRequest::GET);
	}

	void TwitterInterface::requestMoreTweets (const QString& last)
	{
		KQOAuthParameters param;

		qDebug () << "Getting more tweets from " << last;
		param.insert ("max_id", last);
		param.insert ("count", QString ("%1").arg (30));
		SetLastRequestMode (FeedMode::HomeTimeline);
		SignedRequest (TwitterRequest::HomeTimeline, KQOAuthRequest::GET, param);
	}

	void TwitterInterface::requestUserTimeline (const QString& username)
	{
		KQOAuthParameters param;
		param.insert ("screen_name", username);
		SetLastRequestMode (FeedMode::UserTimeline);
		SignedRequest (TwitterRequest::UserTimeline, KQOAuthRequest::GET, param);
	}

	void TwitterInterface::Login (const QString& savedToken, const QString& savedTokenSecret)
	{
		Token_ = savedToken;
		TokenSecret_ = savedTokenSecret;
		qDebug () << "Successfully logged in";
	}

	void TwitterInterface::ReportSPAM (const QString& username, const qulonglong userid)
	{
		KQOAuthParameters param;

		param.insert ("screen_name", username);
		if (userid)
			param.insert ("user_id", QString::number (userid));
		SignedRequest (TwitterRequest::SpamReport, KQOAuthRequest::POST, param);
	}
	
	FeedMode TwitterInterface::GetLastRequestMode () const
	{
		return LastRequestMode_;
	}
	
	void TwitterInterface::SetLastRequestMode (const FeedMode& newLastRequestMode)
	{
		LastRequestMode_ = newLastRequestMode;
	}
	
	void TwitterInterface::request (const KQOAuthParameters& param, const FeedMode mode)
	{
		switch (mode) 
		{
			case FeedMode::UserTimeline:
				SetLastRequestMode (FeedMode::UserTimeline);
				SignedRequest (TwitterRequest::UserTimeline, KQOAuthRequest::GET, param);
				break;
				
			case FeedMode::HomeTimeline:
				SetLastRequestMode (FeedMode::HomeTimeline);
				SignedRequest (TwitterRequest::HomeTimeline, KQOAuthRequest::GET, param);
				break;
		}
	}
}
}

