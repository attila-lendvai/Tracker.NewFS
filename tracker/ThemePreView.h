#ifndef _THEME_PRE_VIEW_H_
#define _THEME_PRE_VIEW_H_

#include <View.h>
#include <Bitmap.h>
#include <Path.h>

#include "ExtendedIcon.h"
#include "IconTheme.h"

class ThemePreView : public BView {

public:
					ThemePreView(BRect frame, const char *name);

	void			SetTo(const char *theme);
	void			Draw(BRect rect);
	float			GetSize();

private:
	void			RebuildBitmap();

	BString			fCurrentTheme;
	BString			fLastTheme;
	BBitmap			*fBitmap;
	float			fSize;
};

#endif
