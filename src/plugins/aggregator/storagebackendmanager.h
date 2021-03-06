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

#include <memory>
#include <QObject>
#include <util/sll/eitherfwd.h>
#include "storagebackend.h"

namespace LeechCraft
{
class StorageBackend;

namespace Aggregator
{
	class StorageBackendManager : public QObject
	{
		Q_OBJECT

		StorageBackend_ptr PrimaryStorageBackend_;

		StorageBackendManager () = default;
	public:
		StorageBackendManager (const StorageBackendManager&) = delete;
		StorageBackendManager& operator= (const StorageBackendManager&) = delete;

		static StorageBackendManager& Instance ();

		struct StorageCreationError
		{
			QString Message_;
		};

		using StorageCreationResult_t = Util::Either<StorageCreationError, StorageBackend_ptr>;
		StorageCreationResult_t CreatePrimaryStorage ();

		StorageBackend_ptr MakeStorageBackendForThread () const;

		void Register (const StorageBackend_ptr&);
	signals:
		void channelAdded (const Channel& channel) const;

		/** @brief Notifies about updated channel information.
		 *
		 * This signal is emitted whenever a channel is updated by any of
		 * the instantiated StorageBackend objects.
		 *
		 * @param[out] channel Pointer to the updated channel.
		 */
		void channelDataUpdated (const Channel& channel) const;

		/** @brief Notifies about updated item information.
		 *
		 * This signal is emitted whenever an item is updated by any of
		 * the instantiated StorageBackend objects.
		 *
		 * @param[out] item Pointer to the updated item.
		 * @param[out] channel Pointer to the channel containing updated
		 * item.
		 */
		void itemDataUpdated (const Item& item, const Channel& channel) const;

		/** @brief Notifies that a number of items was removed.
		 *
		 * This signal is emitted whenever items are removed is updated by
		 * any of the instantiated StorageBackend objects.
		 *
		 * @param[out] items The set of IDs of items that have been
		 * removed.
		 */
		void itemsRemoved (const QSet<IDType_t>&) const;

		void channelRemoved (IDType_t) const;
		void feedRemoved (IDType_t) const;

		void storageCreated ();
	};
}
}
