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

#include "startupfirstpage.h"
#include <typeinfo>
#include <QLineEdit>
#include <QTextCodec>
#include <QComboBox>
#include <util/util.h>
#include "xmlsettingsmanager.h"
#include "core.h"

namespace LeechCraft
{
namespace SeekThru
{
	EngineInfo::EngineInfo (const QString& fn,
			const QString& tags, const QString& name)
	: ResourceFileName_ (fn)
	, DefaultTags_ (tags)
	, Name_ (name)
	{
	}

	StartupFirstPage::StartupFirstPage (QWidget *parent)
	: QWizardPage (parent)
	{
		Ui_.setupUi (this);

		Sets_ ["general"] << EngineInfo ("enwiki.xml",
				"enwiki", QString::fromUtf8 ("Wikipedia: en"));
		Sets_ ["general"] << EngineInfo ("github.xml",
				"programming; sources", QString::fromUtf8 ("GitHub.com"));
		Sets_ ["general"] << EngineInfo ("mininova.xml",
				"torrents; mininova", QString::fromUtf8 ("Mininova"));
		Sets_ ["general"] << EngineInfo ("lastfm.xml",
				"music; lastfm", QString::fromUtf8 ("Last.fm"));
		Sets_ ["ru"] << EngineInfo ("ruwiki.xml",
				"ruwiki", QString::fromUtf8 ("Wikipedia: ru"));
		Sets_ ["ru"] << EngineInfo ("youtube.xml",
				"youtube; videos", QString::fromUtf8 ("Поиск видео YouTube"));
		Sets_ ["ru"] << EngineInfo ("yamarket.xml",
				"shops; yamarket", QString::fromUtf8 ("Яндекс.Маркет"));
		Sets_ ["ru"] << EngineInfo ("yalingvo.xml",
				"dicts", QString::fromUtf8 ("Яндекс.Словари"));

		setTitle ("SeekThru");
		setSubTitle (tr ("Select default search engines"));

		connect (Ui_.LocalizationBox_,
				SIGNAL (currentIndexChanged (const QString&)),
				this,
				SLOT (handleCurrentIndexChanged (const QString&)));

		QMap<QString, int> languages;
		languages ["ru"] = 1;

		QString language = Util::GetLanguage ();
		int selectedIdx = languages.contains (language) ?
			languages [language] :
			0;
		Ui_.LocalizationBox_->setCurrentIndex (selectedIdx);
		handleCurrentIndexChanged (QString ("(") + language + ")");
	}

	void StartupFirstPage::initializePage ()
	{
		connect (wizard (),
				SIGNAL (accepted ()),
				this,
				SLOT (handleAccepted ()));
	}

	void StartupFirstPage::Populate (const QString& title)
	{
		EngineInfos_t engines = Sets_ [title];
		Q_FOREACH (EngineInfo info, engines)
		{
			QStringList strings;
			strings << info.Name_
				<< info.DefaultTags_;
			QTreeWidgetItem *item = new QTreeWidgetItem (Ui_.Tree_, strings);
			item->setData (0, RoleSet, title);
			item->setData (0, RoleFile, info.ResourceFileName_);
			item->setCheckState (0, Qt::Checked);

			QLineEdit *edit = new QLineEdit (Ui_.Tree_);
			edit->setText (info.DefaultTags_);
			Ui_.Tree_->setItemWidget (item, 1, edit);
		}
		if (const auto hdr = Ui_.Tree_->header ())
			hdr->setSectionResizeMode (0, QHeaderView::ResizeToContents);
	}

	void StartupFirstPage::handleAccepted ()
	{
		for (int i = 0; i < Ui_.Tree_->topLevelItemCount (); ++i)
		{
			QTreeWidgetItem *item = Ui_.Tree_->topLevelItem (i);
			if (item->checkState (0) != Qt::Checked)
				continue;

			QString set = item->data (0, RoleSet).toString ();
			QString filename = item->data (0, RoleFile).toString ();
			QString full = QString (":/resources/default/%1/%2")
				.arg (set)
				.arg (filename);
			QFile file (full);
			if (!file.open (QIODevice::ReadOnly))
			{
				qWarning () << Q_FUNC_INFO
					<< "could not open file for read only"
					<< full
					<< file.errorString ();
				continue;
			}

			QString contents = QTextCodec::codecForName ("UTF-8")->
				toUnicode (file.readAll ());
			Core::Instance ().HandleEntity (contents,
					static_cast<QLineEdit*> (Ui_.Tree_->itemWidget (item, 1))->text ());
		}
	}

	void StartupFirstPage::handleCurrentIndexChanged (const QString& text)
	{
		Ui_.Tree_->clear ();
		if (text.endsWith (')'))
		{
			QString selected = text.mid (text.size () - 3, 2);
			Populate (selected);
		}
		Populate ("general");
	}
}
}
