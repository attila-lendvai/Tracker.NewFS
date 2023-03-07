#ifndef TRACKER_CONTEXT_H_
#define TRACKER_CONTEXT_H_

#include "Defines.h"
#include <Menu.h>

class TrackerContext : public BMenu
{
	public:
		TrackerContext(const char *title, menu_layout layout = B_ITEMS_IN_COLUMN);
		virtual void MessageReceived(BMessage *msg);
		virtual	void MouseDown(BPoint where);
};

#endif