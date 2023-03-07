#ifndef _TRACKER_HELPER_H_
#define _TRACKER_HELPER_H_

#include <Application.h>
#include "ExtendedIcon.h"
#include "HelperProtocol.h"

using namespace BPrivate;

class TrackerHelper : public BApplication {
public:
							TrackerHelper();
							~TrackerHelper();

static	int32				HelperEntry(void *arg);
		int32				MainLoop();

private:
		ExtendedIcon		fExtendedIcon;
		port_id				fPort;
		thread_id			fLoop;

		area_id				fDataArea;
		IconRequest			*fRequest;
		uint8				*fData;
};

#endif // _TRACKER_HELPER_H_
