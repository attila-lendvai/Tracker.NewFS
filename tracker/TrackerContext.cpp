#include <printf.h>
#include <Message.h>
#include "TrackerContext.h"

TrackerContext::TrackerContext(const char *title, menu_layout layout)
	: BMenu(title, layout)
{
	printf("used TrackerContext\n");
}

void
TrackerContext::MessageReceived(BMessage *msg)
{
	printf("message\n");
	BMenu::MessageReceived(msg);
}

void
TrackerContext::MouseDown(BPoint where)
{
	printf("mousedown\n");
	BMenu::MouseDown(where);
}