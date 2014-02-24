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

#include "anntreedelegate.h"
#include <QTextLayout>
#include <QStaticText>
#include <QTreeView>
#include <QPainter>
#include <QtDebug>
#include "annmanager.h"

namespace LeechCraft
{
namespace Monocle
{
	AnnTreeDelegate::AnnTreeDelegate (QTreeView *view, QObject *parent)
	: QStyledItemDelegate { parent }
	, View_ { view }
	{
	}

	void AnnTreeDelegate::paint (QPainter *painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
	{
		if (index.data (AnnManager::Role::ItemType) != AnnManager::ItemTypes::AnnItem)
		{
			QStyledItemDelegate::paint (painter, option, index);
			return;
		}

		QStaticText text { GetText (index) };
		text.setTextWidth (option.rect.width ());
		painter->drawStaticText (option.rect.topLeft (), text);
	}

	QSize AnnTreeDelegate::sizeHint (const QStyleOptionViewItem& opt, const QModelIndex& index) const
	{
		if (index.data (AnnManager::Role::ItemType) != AnnManager::ItemTypes::AnnItem)
			return QStyledItemDelegate::sizeHint (opt, index);

		auto option = opt;
		option.initFrom (View_);

		auto parent = index;
		while (parent.isValid ())
		{
			option.rect.setWidth (option.rect.width () - View_->indentation ());
			parent = parent.parent ();
		}

		QStaticText text { GetText (index) };
		text.setTextWidth (option.rect.width ());

		return { option.rect.width (), static_cast<int> (text.size ().height ()) };
	}

	QString AnnTreeDelegate::GetText (const QModelIndex& index) const
	{
		return index.data ().toString ();
	}
}
}
