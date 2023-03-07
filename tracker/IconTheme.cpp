#include "IconTheme.h"
#include "IconCache.h"
#include "Commands.h"
#include "Tracker.h"
#include "Utilities.h"
#include <FindDirectory.h>

namespace BPrivate {

static inline
uint8
brightness_for(uint8 red, uint8 green, uint8 blue)
{
	// brightness = 0.301 * red + 0.586 * green + 0.113 * blue
	// we use for performance reasons:
	// brightness = (308 * red + 600 * green + 116 * blue) / 1024
	return uint8((308 * red + 600 * green + 116 * blue) / 1024);
}

CachedIconThemeEntry::CachedIconThemeEntry(const char *mime)
	:	fMime(mime)
{
	fMime.ToLower();
	fHash = HashString(fMime.String(), 0);
}

const char *
CachedIconThemeEntry::Mime() const
{
	if (fMime.Length() > 0)
		return fMime.String();
	return NULL;
}

uint32
CachedIconThemeEntry::Hash() const
{
	return fHash;
}

int
CachedIconThemeEntry::CompareFunction(const CachedIconThemeEntry *first, const CachedIconThemeEntry *second)
{
	if (first->Hash() > second->Hash())
		return 1;
	else if (first->Hash() < second->Hash())
		return -1;
	else
		return 0;
}

IconTheme::IconTheme(const char *theme)
	:	fInit(B_NO_INIT),
		fRosterReady(false),
		fSVGTranslatorID(0),
		fSVGMessage(),
		fCachedIcons(20, true)
{
	Init(theme);
}

IconTheme::~IconTheme()
{
	fInit = B_ERROR;
}

const char *
IconTheme::FileForID(int32 id)
{
	switch (id) {
		case kResAppIcon: return "tracker/application"; break;
		case kResFileIcon: return "tracker/file"; break;
		case kResFolderIcon: return "tracker/folder"; break;
		case kResTrashIcon: return "tracker/trash"; break;
		case kResTrashFullIcon: return "tracker/trash-full"; break;
		case kResQueryIcon: return "tracker/query"; break;
		case kResQueryTemplateIcon: return "tracker/querytemplate"; break;
		case kResPrinterIcon: return "tracker/printer"; break;
		case kResBarberPoleBitmap: return NULL; break;
		case kResFloppyIcon: return "tracker/floppy"; break;
		case kResHardDiskIcon: return "tracker/harddisk"; break;
		case kResCDIcon: return "tracker/cd"; break;
		case kResBeBoxIcon: return "tracker/disks"; break;
		case kResBookmarkIcon: return "tracker/bookmark"; break;
		case kResPersonIcon: return "tracker/person"; break;
		case kResBrokenLinkIcon: return "tracker/broken-link"; break;
		case kResDeskIcon: return "tracker/desktop"; break;
		case kResHomeDirIcon: return "tracker/dir_home"; break;
		case kResBeosFolderIcon: return "tracker/dir_beos"; break;
		case kResBootVolumeIcon: return "tracker/boot"; break;
		case kResFontDirIcon: return "tracker/dir_fonts"; break;
		case kResAppsDirIcon: return "tracker/dir_apps"; break;
		case kResPrefsDirIcon: return "tracker/dir_prefs"; break;
		case kResMailDirIcon: return "tracker/dir_mail"; break;
		case kResQueryDirIcon: return "tracker/dir_query"; break;
		case kResSpoolFileIcon: return "tracker/spoolfile"; break;
		case kResGenericPrinterIcon: return "tracker/printer"; break;
		case kResDevelopDirIcon: return "tracker/dir_develop"; break;
		case kResDownloadDirIcon: return "tracker/dir_download"; break;
		case kResPersonDirIcon: return "tracker/dir_people"; break;
		case kResUtilDirIcon: return "tracker/dir_util"; break;
		case kResConfigDirIcon: return "tracker/dir_config"; break;
		case kResMoveStatusBitmap: return "tracker/status_move"; break;
		case kResCopyStatusBitmap: return "tracker/status_copy"; break;
		case kResTrashStatusBitmap: return "tracker/status_trash"; break;
		case kResBackNavActive: return "tracker/nav_back"; break;
		case kResBackNavInactive: return "tracker/nav_back"; break;
		case kResForwNavActive: return "tracker/nav_forward"; break;
		case kResForwNavInactive: return "tracker/nav_forward"; break;
		case kResUpNavActive: return "tracker/nav_up"; break;
		case kResUpNavInactive: return "tracker/nav_up"; break;
		case kResBackNavActiveSel: return "tracker/nav_back"; break;
		case kResForwNavActiveSel: return "tracker/nav_forward"; break;
		case kResUpNavActiveSel: return "tracker/nav_up"; break;
		case kResHomeNavActive: return "tracker/nav_home"; break;
		case kResHomeNavActiveSel: return "tracker/nav_home"; break;
		case kResHomeNavInactive: return "tracker/nav_home"; break;
		case kResShareIcon: return "tracker/share"; break;
		default: return NULL; break;
	}
	
	return NULL;
};

button_state
IconTheme::StateForID(int32 id)
{
	switch (id) {
		case kResBackNavActive: return STATE_ACTIVE; break;
		case kResBackNavActiveSel: return STATE_SELECTED; break;
		case kResBackNavInactive: return STATE_INACTIVE; break;
		
		case kResForwNavActive: return STATE_ACTIVE; break;
		case kResForwNavActiveSel: return STATE_SELECTED; break;
		case kResForwNavInactive: return STATE_INACTIVE; break;
		
		case kResUpNavActive: return STATE_ACTIVE; break;
		case kResUpNavActiveSel: return STATE_SELECTED; break;
		case kResUpNavInactive: return STATE_INACTIVE; break;
		
		case kResHomeNavActive: return STATE_ACTIVE; break;
		case kResHomeNavActiveSel: return STATE_SELECTED; break;
		case kResHomeNavInactive: return STATE_INACTIVE; break;
		default: return STATE_ERROR; break;
	}
	
	return STATE_ERROR;
}

status_t
IconTheme::Init(const char *theme)
{
	fInit = B_NO_INIT;
	if (find_directory(B_USER_SETTINGS_DIRECTORY, &fThemesPath, true) != B_OK)
		return B_ERROR;
	
	if (!theme) {
		fEnabled = gTrackerSettings.IconThemeEnabled();
		fCurrentTheme = gTrackerSettings.CurrentIconTheme();
	} else {
		fEnabled = true;
		fCurrentTheme = theme;
	}
	
	fSequence = gTrackerSettings.IconThemeLookupSequence();
	
	fThemesPath.Append(gTrackerSettings.SettingsDirectory());
	fThemesPath.Append("Themes");
	fCurrentThemePath = fThemesPath;
	fCurrentThemePath.Append(fCurrentTheme.String());
	
	fCurrentThemeDir.SetTo(fCurrentThemePath.Path());
	if (fCurrentThemeDir.InitCheck() != B_OK)
		return B_ERROR;
	
	fInit = B_OK;
	PRINT(("currenttheme: %s\n", fCurrentThemePath.Path()));
	return fInit;
}

void
IconTheme::InitRoster()
{
	if (!fRosterReady) {
		fRoster.AddTranslators("/boot/home/config/add-ons/Translators/SVGTranslator");
		translator_id *list;
		int32 count = 0;
		fRoster.GetAllTranslators(&list, &count);
		if (count == 1) {
			fSVGTranslatorID = list[0];
			fRoster.GetConfigurationMessage(fSVGTranslatorID, &fSVGMessage);
			fSVGMessage.ReplaceBool("/dataOnly", true);
			fSVGMessage.ReplaceInt32("/backColor", 0x00777477);
			fSVGMessage.ReplaceInt32("/samplesize", 3);
			fSVGMessage.ReplaceBool("/fitContent", true);
			fSVGMessage.ReplaceBool("/scaleToFit", true);
			fRosterReady = true;
		}
	}
}

void
IconTheme::AddCachedEntry(const char *mime)
{
	// if unique insertion fails, we already covered this mime
	CachedIconThemeEntry *entry = new CachedIconThemeEntry(mime);
	if (!fCachedIcons.BinaryInsertUnique(entry, CachedIconThemeEntry::CompareFunction))
		delete entry;
}

void
IconTheme::AddMimesRecursive(const char *path, BObjectList<CachedIconThemeEntry> *list)
{
	if (!list)
		return;
	
	BDirectory directory(path);
	if (directory.InitCheck() != B_OK)
		return;
	
	directory.Rewind();
	BEntry entry;
	BPath entrypath;
	BPath parent;
	BString supertype = "supertype";
	
	while (directory.GetNextEntry(&entry) == B_OK) {
		if (entry.IsDirectory()) {
			entry.GetPath(&entrypath);
			AddMimesRecursive(entrypath.Path(), list);
			continue;
		}
		
		if (entry.GetPath(&entrypath) != B_OK || entrypath.GetParent(&parent) != B_OK)
			continue;
		
		BString mime = parent.Leaf();
		if (mime.Compare("tracker") == 0)
			continue;
		
		if (supertype.Compare(entrypath.Leaf()) != 0) {
			mime.Append("/");
			mime.Append(entrypath.Leaf());
		}
		
		// if unique insertion fails, we already covered this mime
		CachedIconThemeEntry *entry = new CachedIconThemeEntry(mime.String());
		if (!list->BinaryInsertUnique(entry, CachedIconThemeEntry::CompareFunction))
			delete entry;
	}
}

void
IconTheme::RefreshTheme()
{
	bool needtoreinit = false;
	needtoreinit = needtoreinit || fEnabled != gTrackerSettings.IconThemeEnabled();
	needtoreinit = needtoreinit || fCurrentTheme.Compare(gTrackerSettings.CurrentIconTheme()) != 0;
	needtoreinit = needtoreinit || fSequence.Compare(gTrackerSettings.IconThemeLookupSequence()) != 0;
	
	PRINT(("enabled: %s vs. %s; theme: %s vs. %s; sequence: %s vs. %s; result: we %s need a reinit\n",
		(fEnabled  ? "true" : "false"), (gTrackerSettings.IconThemeEnabled() ? "true" : "false"), fCurrentTheme.String(), settings.CurrentIconTheme(), fSequence.String(), settings.IconThemeLookupSequence(), (needtoreinit ? "do" : "don't")));
	
	if (!needtoreinit)
		return;
	
	Init();
	
	// if we have new mimetypes covered by the new theme, we need to retire
	// the default icon that was cached and did not get into our themedicons
	// list because the former theme did _not_ cover the type
	// all the new mimetypes are added to the existing list and flushed later
	AddMimesRecursive(fCurrentThemePath.Path(), &fCachedIcons);
	
	// we now flush the whole list of cached icons and new icons and let the
	// iconcache delete them
	CachedIconThemeEntry *entry;
	while (fCachedIcons.CountItems() > 0) {
		entry = fCachedIcons.RemoveItemAt(0);
		if (!entry)
			continue;
		
		PRINT(("deleting (%010u) '%s'\n", entry->Hash(), entry->Mime()));
		IconCache::sIconCache->IconChangedCaseLess(entry->Mime());
		delete entry;
	}
	
	// notify tracker of the change so windows will redraw
	TTracker *tracker = dynamic_cast<TTracker *>(be_app);
	if (!tracker)
		return;
	
	BMessage message;
	message.AddBool("ThemesEnabled", fEnabled);
	message.AddString("CurrentTheme", fCurrentTheme);
	tracker->SendNotices(kIconThemeChanged, &message);
}

status_t
IconTheme::LoadBitmap(BFile *source, source_kind kind, icon_size size, BBitmap *&target)
{
	if (!source || source->InitCheck() != B_OK)
		return B_ERROR;
	
	status_t result = B_ERROR;
	BBitmap *unscaled = NULL;
		
	if (!target)
		target = new BBitmap(BRect(0, 0, size - 1, size - 1), B_RGBA32);
	
	switch (kind) {
		case SOURCE_SVG: {
			if (!fRosterReady)
				InitRoster();
			if (!fRosterReady) {
				delete target;
				target = NULL;
				return B_NO_INIT;
			}
			
			BMessage message = fSVGMessage;
			message.ReplaceInt32("/width", size);
			message.ReplaceInt32("/height", size);
			
			BMemoryIO memout(target->Bits(), target->BitsLength());
			result = fRoster.Translate(fSVGTranslatorID, source, &message, &memout, 'bits');
		} break;
		case SOURCE_RASTER: {
			BBitmapStream stream;
			unscaled = BTranslationUtils::GetBitmap(source);
			if (unscaled) {
				result = ScaleBitmap(unscaled, target);
			} else {
				delete target;
				target = NULL;
				return B_ERROR;
			}
		} break;
		default: result = B_ERROR; break;
	}
	
	if (result != B_OK) {
		delete unscaled;
		unscaled = NULL;
		delete target;
		target = NULL;
		return B_ERROR;
	}
	
	return result;
}

status_t
IconTheme::ApplyState(int32 id, BBitmap *&target)
{
	uint32 bitslength = target->BitsLength();
	uint8 *bits = (uint8 *)target->Bits();
	switch (StateForID(id)) {
		case STATE_ERROR: break; // stateless icon
		case STATE_ACTIVE: break; // nothing to do
		case STATE_SELECTED: {
			for (uint32 i = 0; i < bitslength; i += 4) {
				if (*(uint32 *)(bits + i) != B_TRANSPARENT_MAGIC_RGBA32) {
					bits[i + 0] = (uint8)(bits[i + 0] * 0.75);
					bits[i + 1] = (uint8)(bits[i + 1] * 0.75);
					bits[i + 2] = (uint8)(bits[i + 2] * 0.75);
				}
			}
		} break;
		case STATE_INACTIVE: {
			for (uint32 i = 0; i < bitslength; i += 4) {
				if (*(uint32 *)(bits + i) != B_TRANSPARENT_MAGIC_RGBA32) {
					bits[i + 0] = brightness_for(bits[i + 0], bits[i + 1], bits[i + 2]);
					bits[i + 1] = bits[i + 0];
					bits[i + 2] = bits[i + 0];
				}
			}
		} break;
	}
	
	return B_OK;
}

status_t
IconTheme::LoadDefault(int32 id, icon_size size, source_kind kind, BBitmap *&target, bool only_if_match)
{
	status_t result = B_ERROR;
	BBitmap *unscaled = NULL;
	icon_size comp_size = (size > B_LARGE_ICON ? B_LARGE_ICON : size);
	
	switch (kind) {
		case SOURCE_RES_BITMAP: {
			result = GetTrackerResources()->GetBitmapResource(B_MESSAGE_TYPE, id, &unscaled);
			if (result == B_OK && only_if_match && unscaled->Bounds().IntegerWidth() != size) {
				delete target;
				target = NULL;
				return B_ERROR;
			}
		} break;
		case SOURCE_RES_ICON: {
			if (only_if_match && comp_size != size)
				return B_ERROR;
			
			unscaled = new BBitmap(BRect(0, 0, comp_size - 1, comp_size - 1), kDefaultIconDepth);
			result = GetTrackerResources()->GetIconResource(id, comp_size, unscaled);
		} break;
		default:
			return B_ERROR;
	}
	
	if (result != B_OK) {
		delete unscaled;
		unscaled = NULL;
		return B_ERROR;
	}
	
	if (unscaled) {
		int assumed = MIN(unscaled->Bounds().IntegerWidth(), unscaled->Bounds().IntegerHeight());
		if (assumed == size) {
			target = unscaled;
			return result;
		}
	}
	
	if (!target)
		target = new BBitmap(BRect(0, 0, size - 1, size - 1), B_CMAP8);
	
	result = ScaleBitmap(unscaled, target);
	
	if (result != B_OK) {
		delete unscaled;
		delete target;
		target = NULL;
		return B_ERROR;
	}
	
	return result;
}

BPath
IconTheme::GetThemesPath()
{
	return fThemesPath;
}

BPath
IconTheme::GetCurrentThemePath()
{
	return fCurrentThemePath;
}

BPath
IconTheme::GetIconPath(const char *filename, lookup_sequence sequence, icon_size size, source_kind *kind_used)
{
	if (!filename)
		return BPath();
	
	BPath path = fCurrentThemePath;	
	BString sizestr = "";
	source_kind kind = SOURCE_UNKNOWN;
	
	switch (sequence) {
		case USE_SCALABLE: {
			path.Append("svg");
			kind = SOURCE_SVG;
		} break;
		case USE_EXACT_SIZE: {
			sizestr << size;
			path.Append(sizestr.String());
			kind = SOURCE_RASTER;
		} break;
		case SCALE_DOWN_BIGGER:
		case SCALE_UP_SMALLER: {
			BPath temp;
			int newsizeindex = exiconindexfor(size);
			
			while (newsizeindex >= 0 && newsizeindex < exiconcount) {
				temp = path;
				sizestr = "";
				sizestr << exiconsize[newsizeindex];
				newsizeindex += (sequence == SCALE_DOWN_BIGGER ? 1 : -1);
				temp.Append(sizestr.String());
				temp.Append(filename);
				
				if (BEntry(temp.Path()).Exists()) {
					if (kind_used)
						*kind_used = SOURCE_RASTER;
					
					return temp;
				}
			}
		} break;
		default: return BPath(); break;
	}
	
	path.Append(filename);
	if (BEntry(path.Path()).Exists()) {
		if (kind_used)
			*kind_used = kind;
		
		return path;
	}
	
	return BPath();
}

//#define PRINT(x) printf x;

status_t
IconTheme::GetThemeIconForResID(int32 id, icon_size size, BBitmap *&target, source_kind kind)
{
	status_t result = B_ERROR;
	if (fInit != B_OK || !fEnabled) {
		result = LoadDefault(id, size, kind, target, false);
		PRINT(("themes disabled: res: %d; size: %d; kind: %d; target: %x\n", result, size, kind, target));
		return result;
	}
	
	int32 index = 0;
	if (fSequence.Length() == 0) {
		result = LoadDefault(id, size, kind, target, false);
		PRINT(("lookupsequence empty: res: %d; size: %d; kind: %d; target: %x\n", result, size, kind, target));
		return result;
	}
	
	while (result != B_OK && index < fSequence.Length()) {
		lookup_sequence seq = (lookup_sequence)fSequence.ByteAt(index++);
		
		PRINT(("seq: %c = ", seq));
		if (seq == FALLBACK_TO_DEFAULT || seq == USE_EXACT_DEFAULT) {
			result = LoadDefault(id, size, kind, target, seq == USE_EXACT_DEFAULT);
			PRINT(("fallback; res: %d; size: %d; kind: %d; target: %x\n", result, size, kind, target));
		} else {
			source_kind used = SOURCE_UNKNOWN;
			BPath path = GetIconPath(FileForID(id), seq, size, &used);
			BFile file(path.Path(), B_READ_ONLY);
			
			if (file.InitCheck() == B_OK)
				result = LoadBitmap(&file, used, size, target);
			
			if (result == B_OK)
				ApplyState(id, target);
			
			PRINT(("kind: %d; path: %s; res: %d\n", used, path.Path(), result));
		}
	}
	
	if (result != B_OK) {
		result = LoadDefault(id, size, kind, target, false);
		PRINT(("themeing error - failing and falling back: res: %d; size: %d; kind: %d; target: %x\n", result, size, kind, target));
	}
	
	return result;	
}

status_t
IconTheme::GetThemeIconForMime(const char *mime, icon_size size, BBitmap *&target)
{
	PRINT(("try getting icon for mime: %s\n", mime));
	
	status_t result = B_ERROR;
	if (fInit != B_OK || !fEnabled) {
		PRINT(("themes disabled: res: %d; size: %d; target: %x\n", result, size, target));
		return result;
	}
	
	int32 index = 0;
	if (fSequence.Length() == 0) {
		PRINT(("lookupsequence empty: res: %d; size: %d; target: %x\n", result, size, target));
		return result;
	}
	
	while (result != B_OK && index < fSequence.Length()) {
		lookup_sequence seq = (lookup_sequence)fSequence.ByteAt(index++);
		
		PRINT(("seq: %c = ", seq));
		if (seq == FALLBACK_TO_DEFAULT || seq == USE_EXACT_DEFAULT) {
			PRINT(("fallback; res: %d; size: %d; target: %x\n", result, size, target));
			return result;
		} else {
			source_kind used = SOURCE_UNKNOWN;
			BString mime_string = mime;
			mime_string.ToLower();
			
			if (mime_string.FindFirst("/") < 0)
				// we really want the supertype
				mime_string.Append("/supertype");
			
			BPath path = GetIconPath(mime_string.String(), seq, size, &used);
			BFile file(path.Path(), B_READ_ONLY);
			
			if (file.InitCheck() == B_OK)
				result = LoadBitmap(&file, used, size, target);
			
			PRINT(("kind: %d; size: %d; path: %s; res: %d\n", used, size, path.Path(), result));
		}
	}
	
#if DEBUG
	if (result != B_OK) {
		PRINT(("themeing error - failing and falling back: res: %d; size: %d; target: %x\n", result, size, target));
	}
#endif
	
	return result;	
}

inline void
fast_memset(uint32 count, uint32 value, uint32 *address)
{
	asm ("push %%ecx; push %%eax; push %%edi;		# store state
		  mov %0,%%ecx; mov %1,%%eax; mov %2,%%edi;	# fill in variables
		  cld; rep; stosl;							# do the copying
		  pop %%edi; pop %%eax; pop %%ecx;			# restore state"
		  : : "m"(count), "m"(value), "m"(address));
}

status_t
IconTheme::ScaleBitmap(BBitmap *source, BBitmap *target)
{
	if (!source || !target)
		return B_BAD_VALUE;

	BRect bounds = source->Bounds();
	float src_width = bounds.Width();
	float src_height = bounds.Height();
	BRect frame = target->Bounds();
	frame.OffsetTo(0, 0);
	float dst_width = frame.Width();
	float dst_height = frame.Height();

	BBitmap *temp = new BBitmap(frame, target->ColorSpace(), true);
	if (temp->ColorSpace() == B_RGBA32 || temp->ColorSpace() == B_RGB32)
		fast_memset(temp->BitsLength() / 4, 0x00ffffff, (uint32 *)temp->Bits());

	BView *view = new BView(frame, "view", 0, B_SUBPIXEL_PRECISE);
	temp->AddChild(view);

	if (src_width > src_height) {
		float ratio = src_height / src_width;
		frame.bottom = dst_height * ratio;
		frame.top += (dst_height - frame.bottom) / 2;
		frame.bottom += frame.top;
	} else if (src_width < src_height) {
		float ratio = src_width / src_height;
		frame.right = dst_width * ratio;
		frame.left += (dst_width - frame.right) / 2;
		frame.right += frame.left;
	}

	temp->Lock();
	view->DrawBitmap(source, frame);
	view->Sync();
	temp->RemoveChild(view);
	temp->Unlock();
	delete view;

	memcpy(target->Bits(), temp->Bits(), MIN(target->BitsLength(), temp->BitsLength()));
	delete temp;

	return B_OK;
}

// static stuff

static IconTheme *sIconTheme = NULL;

IconTheme *GetIconTheme()
{
	if (!sIconTheme)
		sIconTheme = new IconTheme();

	return sIconTheme;
}

} // namespace BPrivate
