#ifndef _ICON_THEME_H_
#define _ICON_THEME_H_

#include "Bitmaps.h"
#include "ExtendedIcon.h"
#include "TrackerSettings.h"
#include <TranslationKit.h>
#include <Message.h>

namespace BPrivate {

enum lookup_sequence {
	USE_SCALABLE = 'a',
	USE_EXACT_SIZE,
	USE_EXACT_DEFAULT,
	SCALE_DOWN_BIGGER,
	SCALE_UP_SMALLER,
	FALLBACK_TO_DEFAULT
};

enum size_dir {
	GET_SMALLER = -1,
	GET_EXACT = 0,
	GET_BIGGER = 1
};

enum source_kind {
	SOURCE_UNKNOWN = -1,
	SOURCE_SVG = 0,			// scalable format use SVGTranslator
	SOURCE_RASTER,			// raster format - use default translators
	SOURCE_RES_BITMAP,		// resource bitmap
	SOURCE_RES_ICON			// resource icon
};

enum button_state {
	STATE_ERROR = -1,
	STATE_ACTIVE = 0,
	STATE_SELECTED,
	STATE_INACTIVE
};

class CachedIconThemeEntry {
public:
						CachedIconThemeEntry(const char *mime);

		const char		*Mime() const;
		uint32			Hash() const;
static	int				CompareFunction(const CachedIconThemeEntry *first, const CachedIconThemeEntry *second);

private:
		BString			fMime;
		uint32			fHash;
};

class IconTheme {
public:
						IconTheme(const char *theme = NULL);
						~IconTheme();

		status_t		Init(const char *theme = NULL);
		void			InitRoster();
		void			RefreshTheme();

static	const char		*FileForID(int32 id);
static	button_state	StateForID(int32 id);

		BPath			GetThemesPath();
		BPath			GetCurrentThemePath();
		BPath			GetIconPath(const char *filename, lookup_sequence sequence, icon_size size = B_MINI_ICON, source_kind *kind_used = NULL);
		status_t		AppendSize(BPath &to);
		status_t		GetThemeIconForResID(int32 id, icon_size size, BBitmap *&target, source_kind kind = SOURCE_RES_BITMAP);
		status_t		GetThemeIconForMime(const char *mime, icon_size size, BBitmap *&target);

		status_t		LoadBitmap(BFile *source, source_kind kind, icon_size size, BBitmap *&target);
		status_t		LoadDefault(int32 id, icon_size size, source_kind kind, BBitmap *&target, bool only_if_match = false);
		status_t		ApplyState(int32 id, BBitmap *&target);

		// used to update / strip the iconcache after we changed theme
		void			AddCachedEntry(const char *mime);
		void			AddMimesRecursive(const char *path, BObjectList<CachedIconThemeEntry> *list);

static	status_t		ScaleBitmap(BBitmap *source, BBitmap *target);

private:
	status_t			fInit;

	bool				fEnabled;
	BString				fCurrentTheme;
	BString				fSequence;

	BPath				fThemesPath;
	BPath				fCurrentThemePath;
	BDirectory			fCurrentThemeDir;

	BTranslatorRoster	fRoster;
	bool				fRosterReady;
	translator_id		fSVGTranslatorID;
	BMessage			fSVGMessage;

	// in this list we keep all cached entries that use a themed icon. with
	// this info we can call IconCache::IconChanged() when we switch themes
	// so that we do not have to delete the whole cache
	BObjectList<CachedIconThemeEntry>	fCachedIcons;
};

extern _IMPEXP_TRACKER IconTheme *GetIconTheme();

} // namespace BPrivate

using namespace BPrivate;

#endif