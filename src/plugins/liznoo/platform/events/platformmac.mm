/**********************************************************************
 * LeechCraft - modular cross-platform feature rich internet client.
 * Copyright (C) 2006-2015  Georg Rudoy
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

#include "platformmac.h"
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <QTimer>
#include <mach/mach_port.h>
#include <mach/mach_interface.h>
#include <mach/mach_init.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <IOKit/pwr_mgt/IOPM.h>
#include <IOKit/IOMessage.h>
#include <IOKit/ps/IOPowerSources.h>
#include <IOKit/ps/IOPSKeys.h>
#include <CoreFoundation/CFNumber.h>

namespace LeechCraft
{
namespace Liznoo
{
namespace Events
{
	namespace
	{
		void IOCallbackProxy (void *refCon, io_service_t service, natural_t messageType, void *messageArgument)
		{
			auto platform = static_cast<PlatformMac*> (refCon);
			platform->IOCallback (service, messageType, messageArgument);
		}
	}

	PlatformMac::PlatformMac (const ICoreProxy_ptr& proxy, QObject *parent)
	: PlatformLayer (proxy, parent)
	, Port_ (IORegisterForSystemPower (this, &NotifyPortRef_, IOCallbackProxy, &NotifierObject_))
	{
		if (!Port_)
		{
			qWarning () << Q_FUNC_INFO
					<< "unable to register for system power";
			return;
		}

		CFRunLoopAddSource (CFRunLoopGetCurrent (),
				IONotificationPortGetRunLoopSource (NotifyPortRef_),
				kCFRunLoopCommonModes);
	}

	PlatformMac::~PlatformMac ()
	{
		Stop ();
	}

	void PlatformMac::Stop ()
	{
		if (!Port_)
			return;

		CFRunLoopRemoveSource (CFRunLoopGetCurrent (),
				IONotificationPortGetRunLoopSource (NotifyPortRef_),
				kCFRunLoopCommonModes);
		IODeregisterForSystemPower (&NotifierObject_);
		IOServiceClose (Port_);
		IONotificationPortDestroy (NotifyPortRef_);
		Port_ = 0;
	}

	void PlatformMac::IOCallback (io_service_t service, natural_t messageType, void *messageArgument)
	{
		switch (messageType)
		{
		case kIOMessageCanSystemSleep:
			IOCancelPowerChange (Port_, reinterpret_cast<long> (messageArgument));
			break;
		case kIOMessageSystemWillSleep:
			emitGonnaSleep (30000);
			break;
		case kIOMessageSystemHasPoweredOn:
			emitWokeUp ();
			break;
		default:
			break;
		}
	}
}
}
}
