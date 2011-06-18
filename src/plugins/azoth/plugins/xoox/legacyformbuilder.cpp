/**********************************************************************
 * LeechCraft - modular cross-platform feature rich internet client.
 * Copyright (C) 2006-2011  Georg Rudoy
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

#include "legacyformbuilder.h"
#include <boost/bind.hpp>
#include <QWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QVariant>
#include <QLineEdit>
#include <QtDebug>

namespace LeechCraft
{
namespace Azoth
{
namespace Xoox
{
	namespace
	{
		void LineEditActorImpl (QWidget *form, const QXmppElement& elem,
				const QString& fieldLabel)
		{
			QLabel *label = new QLabel (fieldLabel);
			QLineEdit *edit = new QLineEdit (elem.value ());
			edit->setObjectName ("field");
			edit->setProperty ("FieldName", elem.tagName ());
			
			QHBoxLayout *lay = new QHBoxLayout;
			lay->addWidget (label);
			lay->addWidget (edit);
			form->layout ()->addItem (lay);
		}
	}

	LegacyFormBuilder::LegacyFormBuilder ()
	: Widget_ (0)
	{
		Tag2Actor_ ["username"] = boost::bind (LineEditActorImpl,
				_1, _2, tr ("Username:"));
		Tag2Actor_ ["password"] = boost::bind (LineEditActorImpl,
				_1, _2, tr ("Password:"));
		Tag2Actor_ ["registered"] = boost::bind (LineEditActorImpl,
				_1, _2, tr ("Registered:"));
	}
	
	QWidget* LegacyFormBuilder::CreateForm (const QXmppElement& containing,
			QWidget *parent)
	{
		Widget_ = new QWidget (parent);
		Widget_->setLayout (new QVBoxLayout ());

		QXmppElement element = containing.firstChildElement ();
		while (!element.isNull ())
		{
			const QString& tag = element.tagName ();

			if (!Tag2Actor_.contains (tag))
				qWarning () << Q_FUNC_INFO
						<< "unknown tag";
			else
				Tag2Actor_ [tag] (Widget_, element);

			element = element.nextSiblingElement ();
		}
		
		return Widget_;
	}
	
	QList<QXmppElement> LegacyFormBuilder::GetFilledChildren () const
	{
		QList<QXmppElement> result;
		if (!Widget_)
			return result;

		Q_FOREACH (QLineEdit *edit, Widget_->findChildren<QLineEdit*> ("field"))
		{
			QXmppElement elem;
			elem.setTagName (edit->property ("FieldName").toString ());
			elem.setValue (edit->text ());
			result << elem;
		}
		
		return result;
	}
}
}
}
