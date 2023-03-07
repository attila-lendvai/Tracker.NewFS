#include "ThemePreView.h"

ThemePreView::ThemePreView(BRect frame, const char *name)
	:	BView(frame, name, B_FOLLOW_ALL, B_WILL_DRAW),
		fLastTheme(""),
		fBitmap(NULL),
		fSize(0)
{
}

void
ThemePreView::SetTo(const char *theme)
{
	fLastTheme = fCurrentTheme;
	if (fCurrentTheme.Compare(theme) == 0)
		return;
	
	fCurrentTheme = theme;
	RebuildBitmap();
}

void
ThemePreView::RebuildBitmap()
{
	delete fBitmap;
	fBitmap = NULL;
	
	icon_size size = (icon_size)Bounds().IntegerHeight();
	int32 index = 0;
	
	// add all possible mimes unequely
	IconTheme *theme = new IconTheme(fCurrentTheme.String());
	BObjectList<CachedIconThemeEntry> list;
	theme->AddMimesRecursive(theme->GetCurrentThemePath().Path(), &list);
	
	BView view(BRect(0, 0, size, size), "bitmapdrawer", 0, 0);
	fBitmap = new BBitmap(BRect(0, 0, size * list.CountItems() - 1, size - 1), B_RGBA32, true);
	fBitmap->AddChild(&view);
	fBitmap->Lock();
	
	BBitmap *bitmap = NULL; // temporary
	CachedIconThemeEntry *entry = NULL;
	while (list.CountItems() > 0) {
		entry = list.RemoveItemAt(0);
		if (theme->GetThemeIconForMime(entry->Mime(), size, bitmap) == B_OK && bitmap) {
			view.MoveTo(size * index++, 0);
			view.DrawBitmap(bitmap);
		}
		PRINT(("theme entry: %x; mime: %s; index: %d; bitmap: %x\n", entry, entry->Mime(), index, bitmap));
		delete entry;
	}
	
	view.Sync();
	fBitmap->RemoveChild(&view);
	fBitmap->Unlock();
	delete theme;
	delete bitmap;
	
	fSize = (index - 1) * size;
	Invalidate();
}

float
ThemePreView::GetSize()
{
	return fSize;
}

void
ThemePreView::Draw(BRect rect)
{
	if (fBitmap) {
		SetDrawingMode(B_OP_ALPHA);
		DrawBitmap(fBitmap, rect, rect);
	} else {
		SetHighColor(255, 255, 255, 255);
		FillRect(Bounds());
	}
}
