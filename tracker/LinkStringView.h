#ifndef _LINK_STRING_VIEW_H_
#define _LINK_STRING_VIEW_H_

#include <StringView.h>
#include <String.h>

class BLinkStringView : public BStringView {

public:
					BLinkStringView(BRect bounds, const char *name, const char *text, const char *link, uint32 resizeFlags = B_FOLLOW_LEFT | B_FOLLOW_TOP, uint32 flags = B_WILL_DRAW);
					BLinkStringView(BMessage *data);
virtual				~BLinkStringView();
static	BArchivable	*Instantiate(BMessage *data);
virtual	status_t	Archive(BMessage *data, bool deep = true) const;

		void		SetLink(const char *link);
		const char	*Link();
		void		SetLinkColor(rgb_color color);
		rgb_color	LinkColor();

virtual	void		Draw(BRect bounds);
virtual	void		MouseDown(BPoint where);

private:
		BString		fLink;
		rgb_color	fLinkColor;
};

#endif
