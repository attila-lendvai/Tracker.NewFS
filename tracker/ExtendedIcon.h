#ifndef EXTENDED_ICON_H_
#define EXTENDED_ICON_H_

#include <TranslationKit.h>
#include <SupportDefs.h>
#include <Bitmap.h>
#include <Node.h>
#include "Utilities.h"

#define suffixicon	"STD_ICON"
#define prefixicon	"BEOS"
#define prefixmeta	"META"

//#define MULTITHREAD_ICONS
// for multithreading to work HasSVGIcon and/or HasFileIcon need to be impl.!

#define SUPPORT_FAT_ICONS
#ifdef SUPPORT_FAT_ICONS
#define g32icon	":G32:"	/* 128x128x32 */
#define h32icon	":H32:"	/* 64x64x32 */
#define i32icon	":I32:"	/* 48x48x32 */
#define l32icon	":L32:"	/* 32x32x32 */
#define m32icon	":M32:"	/* 16x16x32 */
#define ilicon	":IL:"	/* link to actual icon */
#define faticonpath	"/boot/home/config/icons/"
#endif

#define licon	":L:"	/* 32x32x8 */
#define micon	":M:"	/* 16x16x8 */
#define vicon	":V:"	/* vector icon */

namespace BPrivate {

class Model;

static int exiconsize[] = { 16, 32, 64, 96, 128, 192, 256, 320, 512 };
static int exiconcount = 9;

inline int
exiconindexfor(int size)
{
	for (int i = 0; i < exiconcount; i++)
		if (size == exiconsize[i])
			return i;
	
	// no exact match
	int index = 0;
	int diff = abs(size - exiconsize[0]);
	for (int i = 0; i < exiconcount; i++) {
		if (abs(size - exiconsize[i]) < diff) {
			index = i;
			diff = abs(size - exiconsize[i]);
		}
	}
	return index;
}

enum exicon_source {
	kThumbnailFromAttribute = 0,
	kThumbnailFromFile,
	kSVGIconFromFile,
	kSVGIcon,
	kFileIcon,
	kFallback
};

#ifdef MULTITHREAD_ICONS
struct PendingIcon {
	PendingIcon() : target(target), source(kFallback), model(NULL), node(NULL), prefix(NULL), suffix(NULL), size(B_MINI_ICON) {};
	PendingIcon(BBitmap *&_target, exicon_source _source, Model *_model, BNode *_node, const char *_prefix, const char *_suffix, icon_size _size)
		:	target(_target), source(_source), model(_model), node(_node), prefix(_prefix), suffix(_suffix), size(_size) {};
	BBitmap *&target;
	exicon_source source;
	Model *model;
	BNode *node;
	const char *prefix;
	const char *suffix;
	icon_size size;
};
#endif

class ExtendedIcon {

public:
					ExtendedIcon();
					~ExtendedIcon();

		void		SetTo(Model *model, icon_size size, const char *prefix = prefixicon, const char *suffix = suffixicon);
		void		SetTo(BNode *node, icon_size size, const char *prefix = prefixicon, const char *suffix = suffixicon);
#ifdef MULTITHREAD_ICONS
		void		SetTo(PendingIcon *icon);
#endif

		inline bool	Lock() { return fLock.Lock(); };
		inline void	Unlock() { fLock.Unlock(); };
		inline bool	IsLocked() { return fLock.IsLocked(); };
		inline void CheckLock() { if (!IsLocked()) debugger("The ExtendedIcon was not locked"); };

// public API
		status_t	GetIcon(BBitmap *&target);
		status_t	GetSVGIcon(const char *filename, icon_size size, BBitmap *&target);

// static API
static	status_t	MakeScaledIcon(BBitmap *&bitmap, icon_size size);
static	status_t	ScaleBitmap(BBitmap *source, BBitmap *target, bool border);
static	status_t	ScaleBilinear(BBitmap *source, BBitmap *target);

		status_t	ThreadPendingIcons();
private:

		void		InitRoster();

		status_t	ReadThumbFromBuffer(uint8 *buffer, off_t size, BBitmap *&target);
		status_t	WriteThumbToBuffer(BBitmap *source, uint8 **buffer, off_t *size);

		bool		HasFileIcon();
		status_t	GetFileIcon(BBitmap *&target);
		bool		HasSVGIcon();
		status_t	GetSVGIcon(BBitmap *&target, bool from_file = false);

		status_t	CreateThumb(BBitmap *&target);
		status_t	GetThumbFromAttribute(BBitmap *&target);
		status_t	SaveThumbToAttribute(BBitmap *source);

		void		AllocTarget(BBitmap *&target, icon_size size = (icon_size)0);
		status_t	ReplaceTarget(BBitmap *source, BBitmap *&target);

		Model						*fModel;
		BNode						*fNode;
		icon_size					fSize;
		BRect						fRect; // convinience
		uint32						fType;
		const char					*fPrefix;
		const char					*fSuffix;

#ifdef MULTITHREAD_ICONS
		BObjectList<PendingIcon>	fPendingIcons;
		thread_id					fThreadPending;
#endif

		BTranslatorRoster			fRoster;
		bool						fRosterReady;
		translator_id				fSVGTranslatorID;
		BMessage					fSVGMessage;
		
		Benaphore					fLock;
};

} // namespace BPrivate

#endif
