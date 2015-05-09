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

#include "taskqueuemanager.h"
#include <QMutexLocker>
#include "accountthreadworker.h"
#include "concurrentexceptions.h"

namespace LeechCraft
{
namespace Snails
{
	TaskQueueItem::TaskQueueItem ()
	{
	}

	TaskQueueItem::TaskQueueItem (const QByteArray& method,
			const QList<ValuedMetaArgument>& args, const QByteArray& id)
	: TaskQueueItem { Priority::Normal, method, args, id }
	{
	}

	TaskQueueItem::TaskQueueItem (int priority, const QByteArray& method,
			const QList<ValuedMetaArgument>& args, const QByteArray& id)
	: Priority_ { priority }
	, Method_ { method }
	, Args_ (args)
	, ID_ { id }
	{
	}

	TaskQueueItem::TaskQueueItem (Priority priority,
			const QByteArray& method, const QList<ValuedMetaArgument>& args, const QByteArray& id)
	: TaskQueueItem { static_cast<int> (priority), method, args, id }
	{
	}

	bool operator== (const TaskQueueItem& left, const TaskQueueItem& right)
	{
		return left.Method_ == right.Method_ &&
				left.Args_ == right.Args_;
	}

	QList<TaskQueueManager::TaskMerger> TaskQueueManager::Mergers_
	{
	};

	TaskQueueManager::TaskQueueManager (AccountThreadWorker *worker)
	: ATW_ { worker }
	{
		connect (this,
				SIGNAL (gotTask ()),
				this,
				SLOT (rotateTaskQueue ()),
				Qt::QueuedConnection);
	}

	void TaskQueueManager::RegisterTaskMerger (const TaskMerger& merger)
	{
		Mergers_ << merger;
	}

	void TaskQueueManager::AddTasks (QList<TaskQueueItem> items)
	{
		bool shouldRotateQueue = false;
		{
			QMutexLocker locker { &ItemsMutex_ };

			shouldRotateQueue = Items_.isEmpty ();

			for (const auto& item : items)
			{
				if (Items_.contains (item))
					continue;

				if (!item.ID_.isEmpty () &&
						std::any_of (Items_.begin (), Items_.end (),
								[&item] (const TaskQueueItem& other)
									{ return item.ID_ == other.ID_; }))
					continue;

				const auto pos = std::lower_bound (Items_.begin (), Items_.end (),
						item,
						[] (const TaskQueueItem& left, const TaskQueueItem& right)
							{ return left.Priority_ < right.Priority_; });
				Items_.insert (pos, item);
			}

			if (Items_.isEmpty ())
				return;
		}

		if (shouldRotateQueue)
			emit gotTask ();
	}

	bool TaskQueueManager::HasItems () const
	{
		QMutexLocker locker { &ItemsMutex_ };
		return !Items_.isEmpty ();
	}

	TaskQueueItem TaskQueueManager::PopItem ()
	{
		QMutexLocker locker { &ItemsMutex_ };
		return Items_.isEmpty () ? TaskQueueItem {} : Items_.takeLast ();
	}

	template<typename Ex>
	bool TaskQueueManager::HandleReconnect (const TaskQueueItem& item, const Ex& ex, int recLevel)
	{
		qWarning () << Q_FUNC_INFO
				<< "error while calling"
				<< item.Method_
				<< "with"
				<< item.Args_
				<< ":"
				<< ex.what ();

		if (recLevel >= 3)
		{
			item.Promise_->reportException (MakeWrappedException (ex));
			return false;
		}
		else
		{
			qWarning () << "reconnecting for the"
					<< recLevel
					<< "time";
			ATW_->flushSockets ();
			HandleItem (item, ++recLevel);
			return true;
		}
	}

	void TaskQueueManager::HandleItem (const TaskQueueItem& item, int recLevel)
	{
		if (!item.Promise_->isStarted ())
			item.Promise_->reportStarted ();

		const auto invoke = [this, &item]
		{
			return QMetaObject::invokeMethod (ATW_,
					item.Method_,
					Qt::DirectConnection,
					item.Args_.value (0),
					item.Args_.value (1),
					item.Args_.value (2),
					item.Args_.value (3),
					item.Args_.value (4),
					item.Args_.value (5),
					item.Args_.value (6),
					item.Args_.value (7),
					item.Args_.value (8),
					item.Args_.value (9));
		};

		try
		{
			const bool res = invoke ();

			if (!res)
			{
				qWarning () << Q_FUNC_INFO
						<< "unable to call"
						<< item.Method_
						<< "with"
						<< item.Args_;
				item.Promise_->reportException (InvokeFailedException { item });
			}
		}
		catch (const vmime::exceptions::authentication_error& err)
		{
			const auto& respStr = QString::fromUtf8 (err.response ().c_str ());

			qWarning () << Q_FUNC_INFO
					<< "caught auth error:"
					<< respStr
					<< "while calling"
					<< item.Method_
					<< "with"
					<< item.Args_;

			item.Promise_->reportException (AuthorizationException { respStr });
		}
		catch (const vmime::exceptions::operation_timed_out&)
		{
			qWarning () << Q_FUNC_INFO
					<< "timeout while calling"
					<< item.Method_
					<< "with"
					<< item.Args_;

			item.Promise_->reportException (TimeoutException {});
		}
		catch (const vmime::exceptions::not_connected& e)
		{
			if (HandleReconnect (item, e, recLevel))
				return;
		}
		catch (const vmime::exceptions::socket_exception& e)
		{
			if (HandleReconnect (item, e, recLevel))
				return;
		}
		catch (const std::exception& e)
		{
			qWarning () << Q_FUNC_INFO
					<< "caught"
					<< e.what ()
					<< "while calling"
					<< item.Method_
					<< "with"
					<< item.Args_;
			item.Promise_->reportException (MakeWrappedException (e));
		}

		if (!item.Promise_->isFinished ())
			item.Promise_->reportFinished ();
	}

	void TaskQueueManager::rotateTaskQueue ()
	{
		qDebug () << Q_FUNC_INFO << "start";
		while (HasItems ())
		{
			const auto& item = PopItem ();
			if (item.Method_.isEmpty ())
				break;

			HandleItem (item);
		}
		qDebug () << Q_FUNC_INFO << "done";
	}
}
}
