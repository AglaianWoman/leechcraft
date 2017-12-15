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

#include "visitortest.h"
#include <QtTest>
#include <visitor.h>

QTEST_MAIN (LeechCraft::Util::VisitorTest)

namespace LeechCraft
{
namespace Util
{
	using Variant_t = boost::variant<int, char, std::string, QString, double, float>;

	struct S1
	{
		int field1;
		double field2;
	};

	struct S2
	{
		int field1;
		double field2;
	};

	using SVariant_t = boost::variant<S1, S2>;

	void VisitorTest::testBasicVisitor ()
	{
		Variant_t v { 'a' };
		const auto& res = Visit (v,
					[] (char) { return true; },
					[] (int) { return false; },
					[] (std::string) { return false; },
					[] (QString) { return false; },
					[] (double) { return false; },
					[] (float) { return false; });
		QCOMPARE (res, true);
	}

	void VisitorTest::testBasicVisitorGenericFallback ()
	{
		Variant_t v { 'a' };
		const auto& res = Visit (v,
					[] (char) { return true; },
					[] (int) { return false; },
					[] (auto) { return false; });
		QCOMPARE (res, true);
	}

	void VisitorTest::testBasicVisitorCoercion ()
	{
		Variant_t v { 'a' };
		const auto& res = Visit (v,
					[] (int) { return true; },
					[] (std::string) { return false; },
					[] (QString) { return false; },
					[] (double) { return false; },
					[] (float) { return false; });
		QCOMPARE (res, true);
	}

	void VisitorTest::testBasicVisitorCoercionGenericFallback ()
	{
		Variant_t v { 'a' };
		const auto& res = Visit (v,
					[] (int) { return false; },
					[] (QString) { return false; },
					[] (auto) { return true; });
		QCOMPARE (res, true);
	}

#define NC nc = std::unique_ptr<int> {}

	void VisitorTest::testNonCopyableFunctors ()
	{
		Variant_t v { 'a' };
		const auto& res = Visit (v,
					[NC] (char) { return true; },
					[NC] (int) { return false; },
					[NC] (std::string) { return false; },
					[NC] (QString) { return false; },
					[NC] (double) { return false; },
					[NC] (float) { return false; });
		QCOMPARE (res, true);
	}
#undef NC

	void VisitorTest::testDifferentReturnTypes ()
	{
		struct Foo {};
		struct Bar : Foo {};

		Variant_t v { 'a' };
		decltype (auto) res = Visit (v,
				[] (int) -> Foo* { return nullptr; },
				[] (char) -> Foo* { return nullptr; },
				[] (auto) -> Bar* { return nullptr; });

		decltype (auto) res2 = Visit (v,
				[] (int) -> Bar* { return nullptr; },
				[] (char) -> Bar* { return nullptr; },
				[] (auto) -> Foo* { return nullptr; });

		static_assert (std::is_same_v<decltype (res), Foo*>);
		static_assert (std::is_same_v<decltype (res2), Foo*>);

		QCOMPARE (res, nullptr);
		QCOMPARE (res2, nullptr);
	}

	void VisitorTest::testAcceptsRValueRef ()
	{
		const auto& res = Visit (Variant_t { 'a' },
				[] (char) { return true; },
				[] (auto) { return false; });
		QCOMPARE (res, true);
	}

	void VisitorTest::testLValueRef ()
	{
		Variant_t v { 'a' };
		int ref = 0;
		auto& res = Visit (v, [&ref] (auto) -> int& { return ref; });
		res = 10;
		QCOMPARE (ref, 10);
	}

	void VisitorTest::testLValueRef2 ()
	{
		SVariant_t v { S1 { 0, 0 } };
		Visit (v, [] (auto& s) -> int& { return s.field1; }) = 10;
		const auto& res = Visit (v, [] (const auto& s) -> const int& { return s.field1; });
		QCOMPARE (res, 10);
	}

	void VisitorTest::testPrepareVisitor ()
	{
		Variant_t v { 'a' };
		Visitor visitor
		{
			[] (char) { return true; },
			[] (int) { return false; },
			[] (std::string) { return false; },
			[] (QString) { return false; },
			[] (double) { return false; },
			[] (float) { return false; }
		};

		const auto& res = visitor (v);
		QCOMPARE (res, true);
	}

	void VisitorTest::testPrepareVisitorConst ()
	{
		const Variant_t v { 'a' };
		Visitor visitor
		{
			[] (char) { return true; },
			[] (int) { return false; },
			[] (std::string) { return false; },
			[] (QString) { return false; },
			[] (double) { return false; },
			[] (float) { return false; }
		};

		const auto& res = visitor (v);
		QCOMPARE (res, true);
	}

	void VisitorTest::testPrepareVisitorRValue ()
	{
		Visitor visitor
		{
			[] (char) { return true; },
			[] (int) { return false; },
			[] (std::string) { return false; },
			[] (QString) { return false; },
			[] (double) { return false; },
			[] (float) { return false; }
		};

		const auto& res = visitor (Variant_t { 'a' });
		QCOMPARE (res, true);
	}

	void VisitorTest::testPrepareVisitorFinally ()
	{
		Variant_t v { 'a' };

		bool fin = false;

		auto visitor = Visitor
		{
			[] (char) { return true; },
			[] (auto) { return false; }
		}.Finally ([&fin] { fin = true; });

		const auto& res = visitor (v);
		QCOMPARE (res, true);
		QCOMPARE (fin, true);
	}

	void VisitorTest::testPrepareJustAutoVisitor ()
	{
		using Variant_t = boost::variant<int, double, float>;

		Visitor visitor
		{
			[] (auto e) { return std::to_string (e); }
		};

		const auto& res = visitor (Variant_t { 10 });
		QCOMPARE (res, std::string { "10" });
	}
}
}
