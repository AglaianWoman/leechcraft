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

#include "fb2converter.h"
#include <functional>
#include <memory>
#include <QDomDocument>
#include <QTextDocument>
#include <QTextCursor>
#include <QTextFrame>
#include <QUrl>
#include <QImage>
#include <QApplication>
#include <QPalette>
#include <QVariant>
#include <QStringList>
#include <QtDebug>
#include <util/sll/util.h>
#include "toclink.h"

namespace LeechCraft
{
namespace Monocle
{
namespace FXB
{
	FB2Converter::FB2Converter (Document *doc, const QDomDocument& fb2)
	: ParentDoc_ (doc)
	, FB2_ (fb2)
	, Result_ (new QTextDocument)
	, Cursor_ (new QTextCursor (Result_))
	, SectionLevel_ (0)
	{
		Result_->setPageSize (QSize (600, 800));
		Result_->setUndoRedoEnabled (false);

		const auto& docElem = FB2_.documentElement ();
		if (docElem.tagName () != "FictionBook")
		{
			Error_ = tr ("Invalid FictionBook document.");
			return;
		}

		auto frameFmt = Result_->rootFrame ()->frameFormat ();
		frameFmt.setMargin (20);
		const auto& pal = qApp->palette ();
		frameFmt.setBackground (pal.brush (QPalette::Base));
		Result_->rootFrame ()->setFrameFormat (frameFmt);

		Handlers_ ["section"] = [this] (const QDomElement& p) { HandleSection (p); };
		Handlers_ ["title"] = [this] (const QDomElement& p) { HandleTitle (p); };
		Handlers_ ["subtitle"] = [this] (const QDomElement& p) { HandleTitle (p, 1); };
		Handlers_ ["epigraph"] = [this] (const QDomElement& p) { HandleEpigraph (p); };
		Handlers_ ["image"] = [this] (const QDomElement& p) { HandleImage (p); };

		Handlers_ ["p"] = [this] (const QDomElement& p) { HandlePara (p); };
		Handlers_ ["poem"] = [this] (const QDomElement& p) { HandlePoem (p); };
		Handlers_ ["empty-line"] = [this] (const QDomElement& p) { HandleEmptyLine (p); };
		Handlers_ ["stanza"] = [this] (const QDomElement& p) { HandleStanza (p); };
		Handlers_ ["v"] = [this] (const QDomElement& p)
		{
			auto fmt = Cursor_->blockFormat ();
			fmt.setTextIndent (50);
			Cursor_->insertBlock (fmt);
			HandleParaWONL (p);
		};

		Handlers_ ["emphasis"] = [this] (const QDomElement& p)
		{
			HandleMangleCharFormat (p,
					[] (QTextCharFormat& fmt) { fmt.setFontItalic (true); },
					[this] (const QDomElement& p) { HandleParaWONL (p); });
		};
		Handlers_ ["strong"] = [this] (const QDomElement& p)
		{
			HandleMangleCharFormat (p,
					[] (QTextCharFormat& fmt) { fmt.setFontWeight (QFont::Bold); },
					[this] (const QDomElement& p) { HandleParaWONL (p); });
		};
		Handlers_ ["strikethrough"] = [this] (const QDomElement& p)
		{
			HandleMangleCharFormat (p,
					[] (QTextCharFormat& fmt) { fmt.setFontStrikeOut (true); },
					[this] (const QDomElement& p) { HandleParaWONL (p); });
		};

		Handlers_ ["annotation"] = [this] (const QDomElement& p)
		{
			HandleMangleBlockFormat (p,
					[] (QTextBlockFormat& fmt) -> void
					{
						fmt.setAlignment (Qt::AlignRight);
						fmt.setLeftMargin (60);
					},
					[this] (const QDomElement& p)
					{
						HandleMangleCharFormat (p,
								[] (QTextCharFormat& fmt) { fmt.setFontItalic (true); },
								[this] (const QDomElement& p) { HandleChildren (p); });
					});
		};
		Handlers_ ["style"] = [this] (const QDomElement& p) { HandleParaWONL (p); };
		Handlers_ ["coverpage"] = [this] (const QDomElement& p) { HandleChildren (p); };

		TOCEntry entry =
		{
			ILink_ptr (),
			"root",
			TOCEntryLevel_t ()
		};
		CurrentTOCStack_.push (&entry);

		auto elem = docElem.firstChildElement ();
		while (!elem.isNull ())
		{
			const auto& tagName = elem.tagName ();
			if (tagName == "description")
				HandleDescription (elem);
			else if (tagName == "body")
				HandleBody (elem);

			elem = elem.nextSiblingElement ();
		}

		TOC_ = entry.ChildLevel_;
	}

	FB2Converter::~FB2Converter ()
	{
		delete Cursor_;
	}

	QString FB2Converter::GetError () const
	{
		return Error_;
	}

	QTextDocument* FB2Converter::GetResult () const
	{
		return Result_;
	}

	DocumentInfo FB2Converter::GetDocumentInfo () const
	{
		return DocInfo_;
	}

	TOCEntryLevel_t FB2Converter::GetTOC () const
	{
		return TOC_;
	}

	QDomElement FB2Converter::FindBinary (const QString& refId) const
	{
		const auto& binaries = FB2_.elementsByTagName ("binary");
		for (int i = 0; i < binaries.size (); ++i)
		{
			const auto& elem = binaries.at (i).toElement ();
			if (elem.attribute ("id") == refId)
				return elem;
		}

		return {};
	}

	void FB2Converter::HandleDescription (const QDomElement& elem)
	{
		QStringList handledChildren;
		auto getChildValues = [&elem, &handledChildren] (const QString& nodeName) -> QStringList
		{
			handledChildren << nodeName;

			QStringList result;
			auto elems = elem.elementsByTagName (nodeName);
			for (int i = 0; i < elems.size (); ++i)
			{
				const auto& elem = elems.at (i);
				const auto& str = elem.toElement ().text ();
				if (!str.isEmpty ())
					result << str;
			}
			return result;
		};

		DocInfo_.Genres_ = getChildValues ("genre");
		DocInfo_.Title_ = getChildValues ("book-title").value (0);
		DocInfo_.Keywords_ = getChildValues ("keywords")
				.value (0).split (' ', QString::SkipEmptyParts);

		const auto& dateElem = elem.elementsByTagName ("date").at (0).toElement ();
		DocInfo_.Date_.setDate (QDate::fromString (dateElem.attribute ("value"), Qt::ISODate));

		DocInfo_.Author_ += getChildValues ("first-name").value (0) + " ";
		DocInfo_.Author_ += getChildValues ("last-name").value (0) + " ";
		const auto& email = getChildValues ("email").value (0);
		if (!email.isEmpty ())
			DocInfo_.Author_ += "<" + email + "> ";
		DocInfo_.Author_ += getChildValues ("nickname").value (0);

		DocInfo_.Author_ = DocInfo_.Author_.trimmed ().simplified ();

		FillPreamble ();

		const auto& titleInfo = elem.firstChildElement ("title-info");

		auto childElem = titleInfo.firstChildElement ();
		while (!childElem.isNull ())
		{
			if (!handledChildren.contains (childElem.tagName ()))
				Handle (childElem);
			childElem = childElem.nextSiblingElement ();
		}
	}

	void FB2Converter::HandleBody (const QDomElement& bodyElem)
	{
		HandleChildren (bodyElem);
	}

	void FB2Converter::HandleSection (const QDomElement& tagElem)
	{
		++SectionLevel_;

		CurrentTOCStack_.top ()->ChildLevel_.append (TOCEntry ());
		CurrentTOCStack_.push (&CurrentTOCStack_.top ()->ChildLevel_.last ());

		QStringList chunks;
		auto flushChunks = [this, &chunks] () -> void
		{
			if (!chunks.isEmpty ())
			{
				QTextBlockFormat fmt;
				fmt.setTextIndent (20);
				Cursor_->insertBlock (fmt);

				Cursor_->insertText (chunks.join ("\n"));
				chunks.clear ();
			}
		};

		auto child = tagElem.firstChildElement ();
		while (!child.isNull ())
		{
			if (child.tagName () == "p")
			{
				if (child.childNodes ().size () == 1 && child.firstChild ().isText ())
					chunks << child.firstChild ().toText ().data ();

				child = child.nextSiblingElement ();
				continue;
			}

			flushChunks ();

			Handle (child);
			child = child.nextSiblingElement ();
		}

		flushChunks ();

		CurrentTOCStack_.pop ();

		--SectionLevel_;
	}

	void FB2Converter::HandleTitle (const QDomElement& tagElem, int level)
	{
		auto topFrame = Cursor_->currentFrame ();

		QTextFrameFormat frameFmt;
		frameFmt.setBorder (1);
		frameFmt.setPadding (10);
		frameFmt.setBackground (QColor ("#A4C0E4"));
		Cursor_->insertFrame (frameFmt);

		auto child = tagElem.firstChildElement ();
		while (!child.isNull ())
		{
			const auto& tagName = child.tagName ();

			if (tagName == "empty-line")
				Handlers_ [tagName] ({ child });
			else if (tagName == "p")
			{
				const auto origFmt = Cursor_->charFormat ();

				auto titleFmt = origFmt;
				titleFmt.setFontPointSize (18 - 2 * level - SectionLevel_);
				Cursor_->setCharFormat (titleFmt);

				Handlers_ ["p"] ({ child });

				Cursor_->setCharFormat (origFmt);

				const TOCEntry entry =
				{
					ILink_ptr (new TOCLink (ParentDoc_, Result_->pageCount () - 1)),
					child.text (),
					TOCEntryLevel_t ()
				};
				if (CurrentTOCStack_.top ()->Name_.isEmpty ())
					*CurrentTOCStack_.top () = entry;
			}

			child = child.nextSiblingElement ();
		}

		Cursor_->setPosition (topFrame->lastPosition ());
	}

	void FB2Converter::HandleEpigraph (const QDomElement& tagElem)
	{
		auto child = tagElem.firstChildElement ();
		while (!child.isNull ())
		{
			Handle (child);
			child = child.nextSiblingElement ();
		}
	}

	void FB2Converter::HandleImage (const QDomElement& imageElem)
	{
		const auto& refId = imageElem.attribute ("href").mid (1);
		const auto& binary = FindBinary (refId);
		const auto& imageData = QByteArray::fromBase64 (binary.text ().toLatin1 ());
		const auto& image = QImage::fromData (imageData);

		Result_->addResource (QTextDocument::ImageResource,
				{ "image://" + refId },
				QVariant::fromValue (image));

		Cursor_->insertHtml (QString ("<img src='image://%1'/>").arg (refId));
	}

	void FB2Converter::HandlePara (const QDomElement& tagElem)
	{
		auto fmt = Cursor_->blockFormat ();
		fmt.setTextIndent (20);
		Cursor_->insertBlock (fmt);

		HandleParaWONL (tagElem);
	}

	void FB2Converter::HandleParaWONL (const QDomElement& tagElem)
	{
		auto child = tagElem.firstChild ();
		while (!child.isNull ())
		{
			const auto guard = Util::MakeScopeGuard ([&child] { child = child.nextSibling (); });

			if (child.isText ())
			{
				Cursor_->insertText (child.toText ().data ());
				continue;
			}

			if (!child.isElement ())
				continue;

			Handle (child.toElement ());
		}
	}

	void FB2Converter::HandlePoem (const QDomElement& tagElem)
	{
		HandleChildren (tagElem);
	}

	void FB2Converter::HandleStanza (const QDomElement& tagElem)
	{
		HandleChildren (tagElem);
	}

	void FB2Converter::HandleEmptyLine (const QDomElement&)
	{
		Cursor_->insertText ("\n\n");
	}

	void FB2Converter::HandleChildren (const QDomElement& tagElem)
	{
		auto child = tagElem.firstChildElement ();
		while (!child.isNull ())
		{
			Handle (child);
			child = child.nextSiblingElement ();
		}
	}

	void FB2Converter::Handle (const QDomElement& child)
	{
		const auto& tagName = child.tagName ();
		Handlers_.value (tagName, [&tagName] (const QDomElement&)
					{
						qWarning () << Q_FUNC_INFO
								<< "unhandled tag"
								<< tagName;
					}) (child);
	}

	void FB2Converter::HandleMangleBlockFormat (const QDomElement& tagElem,
			std::function<void (QTextBlockFormat&)> mangler, Handler_f next)
	{
		const auto origFmt = Cursor_->blockFormat ();

		auto mangledFmt = origFmt;
		mangler (mangledFmt);
		Cursor_->insertBlock (mangledFmt);

		next ({ tagElem });

		Cursor_->insertBlock (origFmt);
	}

	void FB2Converter::HandleMangleCharFormat (const QDomElement& tagElem,
			std::function<void (QTextCharFormat&)> mangler, Handler_f next)
	{
		const auto origFmt = Cursor_->charFormat ();

		auto mangledFmt = origFmt;
		mangler (mangledFmt);
		Cursor_->setCharFormat (mangledFmt);

		next ({ tagElem });

		Cursor_->setCharFormat (origFmt);
	}

	void FB2Converter::FillPreamble ()
	{
		auto topFrame = Cursor_->currentFrame ();

		QTextFrameFormat format;
		format.setBorder (2);
		format.setPadding (8);
		format.setBackground (QColor ("#6193CF"));

		if (!DocInfo_.Title_.isEmpty ())
		{
			Cursor_->insertFrame (format);
			QTextCharFormat charFmt;
			charFmt.setFontPointSize (18);
			charFmt.setFontWeight (QFont::Bold);
			Cursor_->insertText (DocInfo_.Title_, charFmt);

			Cursor_->setPosition (topFrame->lastPosition ());
		}

		if (!DocInfo_.Author_.isEmpty ())
		{
			format.setBorder (1);
			Cursor_->insertFrame (format);

			QTextCharFormat charFmt;
			charFmt.setFontPointSize (12);
			charFmt.setFontItalic (true);
			Cursor_->insertText (DocInfo_.Author_, charFmt);

			Cursor_->setPosition (topFrame->lastPosition ());
		}

		Cursor_->insertBlock ();
	}
}
}
}
