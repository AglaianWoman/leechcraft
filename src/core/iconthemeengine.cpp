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

#include "iconthemeengine.h"
#include <algorithm>
#include <QAction>
#include <QPushButton>
#include <QTabWidget>
#include <QIcon>
#include <QDir>
#include <QFile>
#include <QFileInfoList>
#include <QApplication>
#include <QToolButton>
#include <QTimer>
#include <QtDebug>
#include "xmlsettingsmanager.h"
#include "childactioneventfilter.h"
#include "util/util.h"
#include "util/sys/paths.h"

using namespace LeechCraft;

IconThemeEngine::IconThemeEngine ()
{
	QTimer *timer = new QTimer (this);
	connect (timer,
			SIGNAL (timeout ()),
			this,
			SLOT (flushCaches ()));
	timer->start (60000);

#ifdef Q_OS_WIN32
	QIcon::setThemeSearchPaths ({ qApp->applicationDirPath () + "/icons/" });
#elif defined (Q_OS_MAC)
	if (QApplication::arguments ().contains ("-nobundle"))
		QIcon::setThemeSearchPaths ({ "/usr/local/kde4/share/icons/" });
	else
		QIcon::setThemeSearchPaths ({ qApp->applicationDirPath () + "/../Resources/icons/" });
#endif

	const QDir& dir = Util::CreateIfNotExists ("/icons/");
	QIcon::setThemeSearchPaths (QStringList { dir.absolutePath () } + QIcon::themeSearchPaths ());

	FindIconSets ();
}

IconThemeEngine& IconThemeEngine::Instance ()
{
	static IconThemeEngine e;
	return e;
}

QIcon IconThemeEngine::GetIcon (const QString& actionIcon, const QString& actionIconOff)
{
	const auto& namePair = qMakePair (actionIcon, actionIconOff);

	{
		QReadLocker locker { &IconCacheLock_ };
		if (IconCache_.contains (namePair))
			return IconCache_ [namePair];
	}

	if (QIcon::hasThemeIcon (actionIcon) &&
			(actionIconOff.isEmpty () ||
			 QIcon::hasThemeIcon (actionIconOff)))
	{
		auto result = QIcon::fromTheme (actionIcon);
		if (!actionIconOff.isEmpty ())
		{
			const auto& off = QIcon::fromTheme (actionIconOff);
			for (const auto& size : off.availableSizes ())
				result.addPixmap (off.pixmap (size, QIcon::Normal, QIcon::On));
		}

		{
			QWriteLocker locker { &IconCacheLock_ };
			IconCache_ [namePair] = result;
		}

		return result;
	}

#ifdef QT_DEBUG
	qDebug () << Q_FUNC_INFO << "no icon for" << actionIcon << actionIconOff << QIcon::themeName () << QIcon::themeSearchPaths ();
#endif

	return QIcon ();
}

void IconThemeEngine::UpdateIconset (const QList<QAction*>& actions)
{
	FindIcons ();

	for (auto action : actions)
	{
		if (action->menu ())
			UpdateIconset (action->menu ()->actions ());

		if (action->property ("WatchActionIconChange").toBool ())
			action->installEventFilter (this);

		if (!action->property ("ActionIcon").isValid ())
			continue;

		SetIcon (action);
	}
}

void IconThemeEngine::UpdateIconset (const QList<QPushButton*>& buttons)
{
	FindIcons ();

	for (auto button : buttons)
	{
		if (!button->property ("ActionIcon").isValid ())
			continue;

		SetIcon (button);
	}
}

void IconThemeEngine::UpdateIconset (const QList<QTabWidget*>& tabs)
{
	FindIcons ();

	for (const auto tw : tabs)
	{
		const auto& icons = tw->property ("TabIcons").toString ()
			.split (" ", QString::SkipEmptyParts);

		int tab = 0;
		for (const auto& name : icons)
			tw->setTabIcon (tab++, GetIcon (name, {}));
	}
}

void IconThemeEngine::UpdateIconset (const QList<QToolButton*>& buttons)
{
	for (auto button : buttons)
	{
		if (!button->property ("ActionIcon").isValid ())
			continue;

		SetIcon (button);
	}
}

void IconThemeEngine::ManageWidget (QWidget *widget)
{
	UpdateIconset (widget->findChildren<QAction*> ());
	UpdateIconset (widget->findChildren<QPushButton*> ());
	UpdateIconset (widget->findChildren<QTabWidget*> ());

	widget->installEventFilter (new ChildActionEventFilter (widget));
}

void IconThemeEngine::RegisterChangeHandler (const std::function<void ()>& function)
{
	Handlers_ << function;
}

QStringList IconThemeEngine::ListIcons () const
{
	return IconSets_;
}

bool IconThemeEngine::eventFilter (QObject *obj, QEvent *e)
{
	if (e->type () != QEvent::DynamicPropertyChange)
		return QObject::eventFilter (obj, e);

	QAction *act = qobject_cast<QAction*> (obj);
	if (!act)
		return QObject::eventFilter (obj, e);

	SetIcon (act);

	return QObject::eventFilter (obj, e);
}

template<typename T>
void IconThemeEngine::SetIcon (T iconable)
{
	QString actionIcon = iconable->property ("ActionIcon").toString ();
	QString actionIconOff = iconable->property ("ActionIconOff").toString ();

	iconable->setIcon (GetIcon (actionIcon, actionIconOff));
}

void IconThemeEngine::FindIconSets ()
{
	IconSets_.clear ();

	Q_FOREACH (const QString& str, QIcon::themeSearchPaths ())
	{
		QDir dir (str);
		Q_FOREACH (const QString& subdirName, dir.entryList (QDir::Dirs))
		{
			QDir subdir (dir);
			subdir.cd (subdirName);
			if (subdir.exists ("index.theme"))
				IconSets_ << subdirName;
		}
	}
}

void IconThemeEngine::FindIcons ()
{
	QString iconSet = XmlSettingsManager::Instance ()->
		property ("IconSet").toString ();

	if (iconSet != OldIconSet_)
	{
		QIcon::setThemeName (iconSet);

		flushCaches ();
		OldIconSet_ = iconSet;

		for (const auto& handler : Handlers_)
			handler ();
	}
}

void IconThemeEngine::flushCaches ()
{
	IconCache_.clear ();
}
