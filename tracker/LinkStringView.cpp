#include "LinkStringView.h"
#include "FSUtils.h"
#include <Entry.h>
#include <Node.h>
#include <Path.h>

const rgb_color kLinkColor = {0, 0, 220, 255};

BLinkStringView::BLinkStringView(BRect bounds, const char *name, const char *text, const char *link, uint32 resizeFlags = B_FOLLOW_LEFT | B_FOLLOW_TOP, uint32 flags = B_WILL_DRAW)
	:	BStringView(bounds, name, text, resizeFlags, flags),
		fLink(link),
		fLinkColor(kLinkColor)
{
}

BLinkStringView::BLinkStringView(BMessage *data)
	:	BStringView(data)
{
	// TODO
}

BLinkStringView::~BLinkStringView()
{
}

BArchivable *
BLinkStringView::Instantiate(BMessage *data)
{
	// TODO
	return NULL;
}

status_t
BLinkStringView::Archive(BMessage *data, bool deep) const
{
	// TODO
	return B_ERROR;
}

void
BLinkStringView::SetLink(const char *link)
{
	fLink = link;
}

const char *
BLinkStringView::Link()
{
	return fLink.String();
}

void
BLinkStringView::SetLinkColor(rgb_color color)
{
	fLinkColor = color;
}

rgb_color
BLinkStringView::LinkColor()
{
	return fLinkColor;
}

void
BLinkStringView::Draw(BRect bounds)
{
	SetHighColor(fLinkColor);
	BStringView::Draw(bounds);
}

void
BLinkStringView::MouseDown(BPoint where)
{
	if (fLink.Length() <= 0)
		return;
	
	srand(rand());
	BString path("/boot/var/tmp/link-string-view-bookmark-");
	path << rand() % 9000 + 1000; // form a unique filename with -#### suffix
	BString mime("application/x-vnd.Be-bookmark");
	
	BFile file(path.String(), B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
	if (file.InitCheck() != B_OK)
		return;
	
	file.WriteAttrString("BEOS:TYPE", &mime);
	file.WriteAttrString("META:url", &fLink);
	file.Unset();
	
	entry_ref ref;
	BEntry entry(path.String());
	entry.GetRef(&ref);
	BMessage refs(B_REFS_RECEIVED);
	refs.AddRef("refs", &ref);
	
	TrackerLaunch(&refs, false, false);
	// it's ok to leave the file back, temp dir is cleared at reboot
	BStringView::MouseDown(where);
}
