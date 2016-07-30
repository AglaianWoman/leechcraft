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

#pragma once

#include <QObject>
#include <QSize>
#include <QList>
#include <QImage>
#include <QMap>
#include <QElapsedTimer>
#include <QtDebug>
#include <util/sll/qtutil.h>

inline bool operator< (const QSize& s1, const QSize& s2)
{
	return std::make_pair (s1.width (), s1.height ()) < std::make_pair (s2.width (), s2.height ());
}

namespace LeechCraft
{
namespace Poshuku
{
namespace DCAC
{
	class TestBase : public QObject
	{
		Q_OBJECT
	protected:
		QList<QImage> TestImages_;

		QMap<QSize, QList<QImage>> BenchImages_;

		static const int BenchRepsCount = 3;
	protected:
		uchar LMaxDiff (const QImage& image1, const QImage& image2)
		{
			if (image1.size () != image2.size ())
				return std::numeric_limits<uchar>::max ();

			uchar diff = 0;

			for (int y = 0; y < image1.height (); ++y)
			{
				const auto sl1 = image1.scanLine (y);
				const auto sl2 = image2.scanLine (y);

				for (int x = 0; x < image1.width () * 4; ++x)
					diff = std::max<uchar> (diff, std::abs (sl1 [x] - sl2 [x]));
			}

			return diff;
		}

		template<typename Ref, typename F, typename... Args>
		uchar CompareModifying (const QImage& image, Ref&& refFunc, F&& testFunc, Args...)
		{
			auto ref = image;
			refFunc (ref, 1.5);
			auto avx = image;
			testFunc (avx, 1.5);

			return LMaxDiff (ref, avx);
		}

		template<typename F>
		void BenchmarkFunction (F&& f)
		{
			BenchmarkFunctionImpl (std::forward<F> (f), 0);
		}
	private:
		template<typename F, typename = std::result_of_t<F (const QImage&)>>
		void BenchmarkFunctionImpl (F&& f, int)
		{
			for (const auto& pair : Util::Stlize (BenchImages_))
			{
				const auto& list = pair.second;

				for (const auto& image : list)
					f (image);

				QElapsedTimer timer;
				timer.start ();

				int rep = 0;
				for (; rep < BenchRepsCount && timer.nsecsElapsed () < 50 * 1000 * 1000; ++rep)
					for (const auto& image : list)
						f (image);

				qDebug () << pair.first << ": " << timer.nsecsElapsed () / (1000 * rep * list.size ());
			}
		}

		template<typename F>
		void BenchmarkFunctionImpl (F&& f, float)
		{
			for (const auto& pair : Util::Stlize (BenchImages_))
			{
				const auto& list = pair.second;

				for (auto image : list)
					f (image);

				uint64_t counter = 0;
				QElapsedTimer timer;
				timer.start ();

				for (int i = 0; i < BenchRepsCount; ++i)
					for (auto image : list)
					{
						image.detach ();

						QElapsedTimer timer;
						timer.start ();

						f (image);

						counter += timer.nsecsElapsed ();
					}

				qDebug () << pair.first << ": " << counter / (1000 * BenchRepsCount * list.size ());
			}
		}
	private slots:
		void initTestCase ();
	};
}
}
}

#define CHECKFEATURE(x) \
	if (!Util::CpuFeatures {}.HasFeature (Util::CpuFeatures::Feature::x)) \
		QSKIP ("unsupported instruction set", QTest::SkipAll);
