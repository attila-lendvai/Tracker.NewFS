#include "Defines.h"
#include "ExtendedIcon.h"
#include "Model.h"
#include <Node.h>
#include <fs_attr.h>
#include <TranslationKit.h>
#include <Bitmap.h>
#include <iovec.h>
#include <zlib.h>
#include <StopWatch.h>
#include <DataIO.h>

#define SHADOW	0xA0;
#define BORDER	0x00;

#ifdef SUPPORT_FAT_ICONS
const char *faticons[] = {	m32icon,	l32icon,	i32icon,	h32icon,	g32icon };
const int faticonsize[] = {	16,			32,			48,			64,			128 };
#endif

#ifdef MULTITHREAD_ICONS
int32
do_pending_icons(void *data)
{
	ExtendedIcon *icon = (ExtendedIcon *)data;
	if (icon)
		return icon->ThreadPendingIcons();
	return B_ERROR;
}
#endif

ExtendedIcon::ExtendedIcon()
	:	fModel(NULL),
		fNode(NULL),
		fSize(B_MINI_ICON),
		fRect(0, 0, 0, 0),
		fType('zICO'),
		fPrefix(prefixicon),
		fSuffix(suffixicon),
#ifdef MULTITHREAD_ICONS
		fPendingIcons(20, false),
		fThreadPending(-1),
#endif
		fRosterReady(false),
		fSVGTranslatorID(0),
		fSVGMessage(),
		fLock("ExtendedIcon")
{
}

ExtendedIcon::~ExtendedIcon()
{
}

void
ExtendedIcon::InitRoster()
{
#if 0
	if (!fRoster) {
		fRoster = new BTranslatorRoster();
		fRoster->AddTranslators();
		translator_id *translators;
		int32 num_translators;
		fRoster->GetAllTranslators(&translators, &num_translators);
		
		for (int32 i = 0; i < num_translators; i++) {
			const translation_format *fmts;
			int32 num_fmts;
			fRoster->GetInputFormats(translators[i], &fmts, &num_fmts);
			float best_png = 0;
			float best_svg = 0;
			
			for (int32 j = 0; j < num_fmts; j++) {
				if (strcasecmp(fmts[j].MIME, "image/png") == 0 && fmts[j].capability > best_png) {
					fPNGTranslatorID = translators[i];
					best_png = fmts[j].capability;
				} else if (strcasecmp(fmts[j].MIME, "image/svg+xml") == 0 && fmts[j].capability > best_svg) {
					best_svg = fmts[j].capability;
					fSVGTranslatorID = translators[i];
				}
			}
		}
		PRINT(("InitRoster(): roster: %x; fPNGTranslatorID: %d; fSVGTranslatorID: %d\n", fRoster, fPNGTranslatorID, fSVGTranslatorID));
		
		fRoster->GetConfigurationMessage(fPNGTranslatorID, &fPNGMessage);
		fPNGMessage.ReplaceInt32(B_TRANSLATOR_EXT_BITMAP_COLOR_SPACE, B_RGBA32);
		
		fRoster->GetConfigurationMessage(fSVGTranslatorID, &fSVGMessage);
		fSVGMessage.ReplaceBool("/dataOnly", true);
		fSVGMessage.ReplaceInt32("/backColor", 0x00777477);
		fSVGMessage.ReplaceInt32("/samplesize", 3);
		fSVGMessage.ReplaceBool("/fitContent", true);
		fSVGMessage.ReplaceBool("/scaleToFit", true);
	}
#else
	// init roster
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
#endif
}

void
ExtendedIcon::SetTo(Model *model, icon_size size, const char *prefix, const char *suffix)
{
	CheckLock();
	fModel = model;
	fNode = NULL;
	fSize = size;
	fRect.Set(0, 0, size - 1, size - 1);
	fPrefix = prefix;
	fSuffix = suffix;
}

void
ExtendedIcon::SetTo(BNode *node, icon_size size, const char *prefix, const char *suffix)
{
	CheckLock();
	fModel = NULL;
	fNode = node;
	fSize = size;
	fRect.Set(0, 0, size - 1, size - 1);
	fPrefix = prefix;
	fSuffix = suffix;
}

#ifdef MULTITHREAD_ICONS
void
ExtendedIcon::SetTo(PendingIcon *icon)
{
	CheckLock();
	fModel = icon->model;
	fNode = icon->node;
	fSize = icon->size;
	fRect.Set(0, 0, fSize - 1, fSize - 1);
	fPrefix = icon->prefix;
	fSuffix = icon->suffix;
}
#endif

//#define PRINT(x)	printf x

status_t
ExtendedIcon::GetIcon(BBitmap *&target)
{
	PRINT(("ExtendedIcon::GetIcon(%08x): model: %08x; node: %08x; size: %d; prefix: %s; suffix: %s; name: %s\n", target, fModel, fNode, fSize, fPrefix, fSuffix, (fModel ? fModel->Name() : "(null)")));
	CheckLock();
	
	if (!fNode && fModel)
		fNode = fModel->Node();
	if (!fNode) {
		PRINT(("could not open node\n"));
		return B_NO_INIT;
	}
	
#ifdef SUPPORT_FAT_ICONS
	BString stringAttr;
	stringAttr << fPrefix << ilicon << fSuffix;
	attr_info info;
	const char *attr = stringAttr.String();
	BNode *oldnode = NULL;
	const char *oldprefix = NULL;
	
	if (fNode->GetAttrInfo(attr, &info) == B_OK && info.size > 0) {
		char *buffer = new char[info.size + 1];
		buffer[info.size] = 0;
		if (fNode->ReadAttr(attr, info.type, 0, buffer, info.size) == info.size) {
			oldnode = fNode;
			fNode = new BNode(buffer);
			
			oldprefix = fPrefix;
			fPrefix = prefixicon;
			
			if (fNode->InitCheck() != B_OK) {
				BString pathstring;
				pathstring << faticonpath << buffer;
				fNode->SetTo(pathstring.String());
				if (fNode->InitCheck() != B_OK) {
					delete fNode;
					fNode = oldnode;
					oldnode = NULL;
				}
			}
		}
	}
#endif
	//printf("%s\n", (fModel ? fModel->Name() : "(nomodel)"));

	status_t result = B_ERROR;
	
	// get a thumbnail if this is an image
	if (fModel && result != B_OK) {
		BString name(fModel->MimeType());
		if (name.Compare("image/", 6) == 0) {
			// use SVG-Rendering if it is a SVG-File
			if (name.Compare("image/svg+xml", 13) == 0) {
#ifdef MULTITHREAD_ICONS
				AllocTarget(target);
				fPendingIcons.AddItem(new PendingIcon(target, kSVGIconFromFile, fModel, fNode, fPrefix, fSuffix, fSize));
				result = B_OK;
#else
				result = GetSVGIcon(target, true);
#endif
#ifdef MULTITHREAD_ICONS
			} else {
				AllocTarget(target);
				fPendingIcons.AddItem(new PendingIcon(target, kThumbnailFromAttribute, fModel, fNode, fPrefix, fSuffix, fSize));
				result = B_OK;
			}
#else
			}
			
			if (result != B_OK)
				result = GetThumbFromAttribute(target);
			
			if (result != B_OK)
				result = CreateThumb(target);
#endif
#ifdef MULTITHREAD_ICONS
		}
	} else {
		if (HasSVGIcon() || HasFileIcon()) {
			AllocTarget(target);
			fPendingIcons.AddItem(new PendingIcon(target, kSVGIcon, fModel, fNode, fPrefix, fSuffix, fSize));
			result = B_OK;
		}
	}
#else
		}
	}
	
	if (result != B_OK)
		result = GetSVGIcon(target);
	
	if (result != B_OK)
		result = GetFileIcon(target);
#endif
	
#ifdef SUPPORT_FAT_ICONS
	if (oldnode) {
		delete fNode;
		fNode = oldnode;
		oldnode = NULL;
	}
	
	if (oldprefix) {
		fPrefix = oldprefix;
		oldprefix = NULL;
	}
#endif

#ifdef MULTITHREAD_ICONS
	if (fThreadPending < 0 && fPendingIcons.CountItems() > 0) {
		PRINT(("about to spawn pending icon thread (%d, %d)\n", fThreadPending, fPendingIcons.CountItems()));
		fThreadPending = spawn_thread(do_pending_icons, "Pending Icons", B_NORMAL_PRIORITY, (void *)this);
		resume_thread(fThreadPending);
		PRINT(("spawned pending icon thread\n"));
	}
#endif
	PRINT(("result: %d; target: %08x; bounds: %f, %f, %f, %f; cs: %d\n", result, target, target ? target->Bounds().left : -1, target ? target->Bounds().top : -1, target ? target->Bounds().right : -1, target ? target->Bounds().bottom : -1, target ? target->ColorSpace() : -1));
	
	return result;
}

bool
ExtendedIcon::HasFileIcon()
{
#ifdef SUPPORT_FAT_ICONS
	if (fModel)
		fNode = fModel->Node();
	if (!fNode)
		return false;
	
	BString stringAttr;
	stringAttr << fPrefix << vicon << fSuffix;
	const char *attr = stringAttr.String();
	attr_info info;
	
	int index;
	switch (fSize) {
		case 16: index = 0; break;
		case 32: index = 1; break;
		case 48: index = 2; break;
		case 64: index = 3; break;
		default: index = 4; break; // everything bigger
	}
	
	int oldindex = index;
	for (int i = 1; i >= -1; i -= 2) {
		while ((i > 0 && index <= 4) || (i < 0 && index >= 0)) {
			stringAttr.SetTo("");
			stringAttr << fPrefix << faticons[index] << fSuffix;
			attr = stringAttr.String();
			
			if (fNode->GetAttrInfo(attr, &info) == B_OK && info.size > 0)
				return true;
			
			index += i;
		}
		
		index = oldindex - 1;
	}
#endif
	
	return false;
}

status_t
ExtendedIcon::GetFileIcon(BBitmap *&target)
{
	if (!fNode && fModel)
		fNode = fModel->Node();
	if (!fNode)
		return B_NO_INIT;
	
	BString stringAttr;
	const char *attr = NULL;
	attr_info info;
	
#ifdef SUPPORT_FAT_ICONS
	int index;
	switch (fSize) {
		case 16: index = 0; break;
		case 32: index = 1; break;
		case 48: index = 2; break;
		case 64: index = 3; break;
		default: index = 4; break; // everything bigger
	}
	
	bool found = false;
	int oldindex = index;
	for (int i = 1; i >= -1; i -= 2) {
		while (!found && ((i > 0 && index <= 4) || (i < 0 && index >= 0))) {
			stringAttr.SetTo("");
			stringAttr << fPrefix << faticons[index] << fSuffix;
			attr = stringAttr.String();
			
			if (fNode->GetAttrInfo(attr, &info) == B_OK && info.size > 0) {
				found = true;
				break;
			}
			index += i;
		}
		
		if (!found)
			index = oldindex - 1;
	}
	
	while (found) {
		uint8 *buffer = new uint8[info.size];
		if (fNode->ReadAttr(attr, info.type, 0, buffer, info.size) != info.size)
			break;
		
		int size = faticonsize[index];
		BBitmap *temp = new BBitmap(BRect(0, 0, size - 1, size - 1), B_RGBA32);
		
		uint32 len = MIN(*((uint32 *)buffer), (uint32)temp->BitsLength());
		zlib::uncompress((uint8 *)temp->Bits(), &len, &buffer[4], info.size - 4);
		delete buffer;
		
		if (fSize != size)
			if (MakeScaledIcon(temp, fSize) != B_OK)
				break;
		
		if (!target)
			target = temp;
		else {
			ReplaceTarget(temp, target);
			delete temp;
			temp = NULL;
		}
		
		return B_OK;
	}
	
	// fallback
#endif
	
	stringAttr.SetTo("");
	stringAttr << fPrefix << (fSize == B_MINI_ICON ? micon : licon) << fSuffix;
	attr = stringAttr.String();
	
	if (fNode->GetAttrInfo(attr, &info) != B_OK || info.size <= 0)
		return B_ERROR;
	
	icon_size comp_size = (fSize == B_MINI_ICON ? B_MINI_ICON : B_LARGE_ICON);
	BBitmap *temp = new BBitmap(BRect(0, 0, comp_size - 1, comp_size - 1), kDefaultIconDepth);
	
	if (fNode->ReadAttr(attr, info.type, 0, temp->Bits(), MIN(info.size, temp->BitsLength())) != info.size)
		return B_ERROR;
	
	if (MakeScaledIcon(temp, fSize) != B_OK)
		return B_ERROR;
	
	if (!target)
		target = temp;
	else {
		ReplaceTarget(temp, target);
		delete temp;
		temp = NULL;
	}
	
	return B_OK;
}

bool
ExtendedIcon::HasSVGIcon()
{
	if (fModel)
		fNode = fModel->Node();
	if (!fNode)
		return false;
	
	BString stringAttr;
	stringAttr << fPrefix << vicon << fSuffix;
	const char *attr = stringAttr.String();
	
	attr_info info;
	if (fNode->GetAttrInfo(attr, &info) == B_OK && info.size > 0)
		return true;
	
	return false;
}

status_t
ExtendedIcon::GetSVGIcon(const char *filename, icon_size size, BBitmap *&target)
{
	InitRoster();
	if (!fRosterReady)
		return B_NO_INIT;
	
	BFile *file = new BFile(filename, B_READ_ONLY);
	if (!file->IsReadable())
		return B_NO_INIT;
	
	off_t filesize = 0;
	if (file->GetSize(&filesize) != B_OK || filesize <= 0)
		return B_ERROR;
	
	uint8 *buffer = new uint8[filesize];
	if (file->Read(buffer, filesize) != filesize) {
		delete [] buffer;
		return B_ERROR;
	}
	
	if (!buffer || filesize <= 0)
		return B_ERROR;
	
	AllocTarget(target, size);
	BMemoryIO memin(buffer, filesize);
	BMemoryIO memout(target->Bits(), target->BitsLength());
	
	Lock();
	fSVGMessage.ReplaceInt32("/width", size);
	fSVGMessage.ReplaceInt32("/height", size);
	status_t result = fRoster.Translate(fSVGTranslatorID, &memin, &fSVGMessage, &memout, 'bits');
	Unlock();
	delete [] buffer;
	
	if (result != B_OK) {
		delete target;
		target = NULL;
	}
	
	PRINT(("GetSVGIcon(%s, %d, %x): result: %d\n", filename, size, target, result));
	return result;
}

status_t
ExtendedIcon::GetSVGIcon(BBitmap *&target, bool from_file)
{
	InitRoster();
	if (!fRosterReady)
		return B_NO_INIT;
	
	uint8 *buffer = NULL;
	status_t result = B_OK;
	off_t size = 0;
	
	if (!from_file) {
		if (fModel)
			fNode = fModel->Node();
		if (!fNode)
			return B_NO_INIT;
		
		BString stringAttr;
		stringAttr << fPrefix << vicon << fSuffix;
		const char *attr = stringAttr.String();
		
		attr_info info;
		if (fNode->GetAttrInfo(attr, &info) != B_OK || info.size <= 0)
			return B_ERROR;
		
		size = info.size;
		buffer = new uint8[size];
		if (fNode->ReadAttr(attr, info.type, 0, buffer, info.size) != info.size) {
			delete [] buffer;
			return B_ERROR;
		}
	} else {
		if (!fModel)
			return B_NO_INIT;
		
		BFile *file = new BFile(fModel->EntryRef(), B_READ_ONLY);
		if (!file->IsReadable())
			return B_NO_INIT;
		
		if (file->GetSize(&size) != B_OK || size <= 0)
			return B_ERROR;
		
		buffer = new uint8[size];
		if (file->Read(buffer, size) != size) {
			delete [] buffer;
			return B_ERROR;
		}
	}
	
	if (!buffer || size <= 0)
		return B_ERROR;
	
	AllocTarget(target);
	
	fSVGMessage.ReplaceInt32("/width", fSize);
	fSVGMessage.ReplaceInt32("/height", fSize);
	
	BMemoryIO memin(buffer, size);
	BMemoryIO memout(target->Bits(), target->BitsLength());
	result = fRoster.Translate(fSVGTranslatorID, &memin, &fSVGMessage, &memout, 'bits');
	delete [] buffer;
	
	PRINT(("GetSVGIcon(%x): result: %d\n", target, result));
	return result;
}

status_t
ExtendedIcon::ReadThumbFromBuffer(uint8 *buffer, off_t size, BBitmap *&target)
{
	if (target)
		delete target;
	
	target = new BBitmap(BRect(0, 0, fSize - 1, fSize - 1), B_RGBA32);
	uint32 length = target->BitsLength();
	if (zlib::uncompress((uint8 *)target->Bits(), &length, buffer, size) == Z_OK)
		return B_OK;
	
	return B_ERROR;
#if 0
	InitRoster();
	if (!fRoster || !source)
		return B_NO_INIT;
	
	if (target)
		delete target;
	
	target = BTranslationUtils::GetBitmap(source, fRoster);
	
	if (target) {
		printf("readthumbfrombuffer:\n");
		target->Bounds().PrintToStream();
		printf("colorspace: ");
		switch (target->ColorSpace()) {
			case B_RGB15: printf("rgb15\n"); break;
			case B_RGBA15: printf("rgba15\n"); break;
			case B_RGB16: printf("rgb16\n"); break;
			case B_RGB24: printf("rgb24\n"); break;
			case B_RGB32: printf("rgb32\n"); break;
			case B_RGBA32: printf("rgba32\n"); break;
			default: printf("unknown format: %d\n", target->ColorSpace()); break;
		}
	}
	
	if (target)
		return B_OK;
	
	return B_ERROR;
#endif
}

status_t
ExtendedIcon::WriteThumbToBuffer(BBitmap *source, uint8 **buffer, off_t *size)
{
	uint32 length = (uint32)(source->BitsLength() * 1.1 + 12);
	*buffer = new uint8[length];
	if (zlib::compress(*buffer, &length, (uint8 *)source->Bits(), source->BitsLength()) == Z_OK) {
		*size = length;
		return B_OK;
	}
	
	delete [] *buffer;
	*size = 0;
	return B_ERROR;
#if 0
	InitRoster();
	if (!fRoster || !source || !target)
		return B_NO_INIT;
	
	BBitmap *temp = new BBitmap(source);
	
	if (source) {
		printf("writethumbtobuffer:\n");
		source->Bounds().PrintToStream();
		printf("colorspace: ");
		switch (source->ColorSpace()) {
			case B_RGB15: printf("rgb15\n"); break;
			case B_RGBA15: printf("rgba15\n"); break;
			case B_RGB16: printf("rgb16\n"); break;
			case B_RGB24: printf("rgb24\n"); break;
			case B_RGB32: printf("rgb32\n"); break;
			case B_RGBA32: printf("rgba32\n"); break;
			default: printf("unknown format: %d\n", source->ColorSpace()); break;
		}
	}
	
	BBitmapStream stream(temp);
	status_t result = fRoster->Translate(fPNGTranslatorID, &stream, &fPNGMessage, target, fType);
	stream.DetachBitmap(NULL);
	delete temp;
	temp = NULL;
	
	return result;
#endif
}

status_t
ExtendedIcon::CreateThumb(BBitmap *&target)
{
	// try to locate a bigger thumbnail and scale this one
	//BStopWatch *creationtime = new BStopWatch("creationtime");
	if (!fNode && fModel)
		fNode = fModel->Node();
	if (!fNode)
		return B_NO_INIT;
	
	// add an attribute that says we are creating an thumbnail and remove it
	// when we're done. If such an attribute is present, this means the last
	// time we were not able to finish the process (maybe the translator
	// crashed and the tracker with it or there was no translator).
	attr_info info;
	const char *attr = "_trk/createthumb";
	time_t lasttime = time(NULL);
	if (fNode->GetAttrInfo(attr, &info) == B_OK && info.size > 0) {
		fNode->ReadAttr(attr, B_INT32_TYPE, 0, &lasttime, 4);
		printf("avoiding thumbnail creation of \"%s\" since it failed %s", fModel->Name(), ctime(&lasttime));
		return B_ERROR;
	}
	
	fNode->WriteAttr(attr, B_INT32_TYPE, 0, &lasttime, 4);
	
	/*for (int i = exiconindexfor((int)fRect.Width() + 1) + 1; i < exiconcount; i++) {
		BString stringAttr;
		stringAttr << fPrefix << ":" << exiconsize[i] << ":" << fSuffix;
		const char *attr = stringAttr.String();
		
		attr_info info;
		if (fNode->GetAttrInfo(attr, &info) != B_OK || info.size <= 0)
			continue;
		
		// if we get here, we found a bigger one
		BBitmap *bitmap = new BBitmap(BRect(0, 0, exiconsize[i] - 1, exiconsize[i] - 1), B_RGBA32);
		if (fNode->ReadAttr(attr, info.type, 0, bitmap->Bits(), MIN(info.size, bitmap->BitsLength())) != info.size) {
			delete bitmap;
			continue;
		}
		
		if (!target)
			target = new BBitmap(fRect, B_RGBA32);
		
		status_t result = ScaleBitmap(bitmap, target, true);
		delete bitmap;
		
		if (result == B_OK) {
			printf("created new thumbnail from thumbnail %d\n", exiconsize[i]);
			SaveThumbToAttribute(target);
			delete creationtime;
			return B_OK;
		} else
			continue;
	}*/
	
	// if there was no bigger thumbnail scale down the original file
	if (!fModel)
		return B_NO_INIT;
	
	BBitmap *bitmap = BTranslationUtils::GetBitmap(fModel->EntryRef());
	if (!bitmap)
		return B_ERROR;
	
	AllocTarget(target);
	status_t result = ScaleBitmap(bitmap, target, true);
	delete bitmap;
	
	if (result == B_OK) {
		if (fModel && !fNode)
			fNode = fModel->Node();
		if (fNode)
			SaveThumbToAttribute(target);
	}
	
	//delete creationtime;
	fNode->RemoveAttr(attr);
	return result;
}

status_t
ExtendedIcon::GetThumbFromAttribute(BBitmap *&target)
{
	if (!fNode && fModel)
		fNode = fModel->Node();
	if (!fNode)
		return B_NO_INIT;
	
	BString stringAttr;
	stringAttr << fPrefix << ":" << fSize << ":" << fSuffix;
	const char *attr = stringAttr.String();
	
	attr_info info;
	if (fNode->GetAttrInfo(attr, &info) != B_OK || info.size <= 0)
		return B_ERROR;
	
	uint8 *buffer = new uint8[info.size];
	if (fNode->ReadAttr(attr, info.type, 0, buffer, info.size) != info.size) {
		delete [] buffer;
		return B_ERROR;
	}
	
	status_t result = ReadThumbFromBuffer(buffer, info.size, target);
	delete [] buffer;
	return result;
}

status_t
ExtendedIcon::SaveThumbToAttribute(BBitmap *source)
{
	if (!fNode && fModel)
		fNode = fModel->Node();
	if (!fNode)
		return B_NO_INIT;
	
	BString stringAttr;
	stringAttr << fPrefix << ":" << fSize << ":" << fSuffix;
	const char *attr = stringAttr.String();
	
	uint8 *buffer = NULL;
	off_t size;
	if (WriteThumbToBuffer(source, &buffer, &size) != B_OK)
		return B_ERROR;
	
	fNode->RemoveAttr(attr);
	if (fNode->WriteAttr(attr, fType, 0, buffer, size) == size)
		return B_OK;
	
	delete [] buffer;
	return B_ERROR;
}

void
ExtendedIcon::AllocTarget(BBitmap *&target, icon_size size)
{
	BRect rect = fRect;
	if (size > 0)
		rect.Set(0, 0, size - 1, size - 1);
		
	if (target) {
		if (target->Bounds().OffsetToSelf(0, 0) == rect
			&& target->ColorSpace() == B_RGBA32)
			return;
		
		delete target;
		target = NULL;
	}
	
	target = new BBitmap(rect, B_RGBA32);
}

status_t
ExtendedIcon::ReplaceTarget(BBitmap *source, BBitmap *&target)
{
	if (!source)
		return B_ERROR;
	
	if (target) {
		if (target->Bounds() != source->Bounds()
			|| target->ColorSpace() != source->ColorSpace()) {
			
			delete target;
			target = NULL;
		}
	}
	
	if (!target)
		target = new BBitmap(source->Bounds(), source->ColorSpace());
	
	memcpy(target->Bits(), source->Bits(), MIN(source->BitsLength(), target->BitsLength()));
	return B_OK;
}

#ifdef MULTITHREAD_ICONS
// thread function
#include <View.h>

status_t
ExtendedIcon::ThreadPendingIcons()
{
	PendingIcon *item = NULL;
	ExtendedIcon icon;
	
	while (true) {
		printf("count: %ld\n", fPendingIcons.CountItems());
		if (fPendingIcons.CountItems() > 0)
			item = fPendingIcons.RemoveItemAt((int32)0);
		
		if (!item) {
			fThreadPending = -1;
			exit_thread(B_OK);
			break;
		}
		
		icon.SetTo(item);
		printf("source: %ld\n", item->source);
		
		bool addagain = false;
		
		switch (item->source) {
			case kThumbnailFromAttribute: {
				if (icon.GetThumbFromAttribute(item->target) != B_OK) {
					item->source = kThumbnailFromFile;
					addagain = true;
				}
			} break;
			
			case kThumbnailFromFile: {
				if (icon.CreateThumb(item->target) != B_OK) {
					item->source = kSVGIcon;
					addagain = true;
				}
			} break;
			
			case kSVGIconFromFile:
			case kSVGIcon: {
				if (icon.GetSVGIcon(item->target, item->source == kSVGIconFromFile) != B_OK) {
					item->source = kFileIcon;
					addagain = true;
				}
			} break;
			
			case kFileIcon: {
				if (icon.GetFileIcon(item->target) != B_OK) {
					item->source = kFallback;
					addagain = true;
				}
			} break;
			
			case kFallback:
			default: break; // don't know what to do
		}
		
		if (addagain)
			fPendingIcons.AddItem(item);
		else
			delete item;
		
		item = NULL;
	}
	
	return B_OK;
}
#endif

// statics

status_t
ExtendedIcon::MakeScaledIcon(BBitmap *&bitmap, icon_size size)
{
	BBitmap *scaled = new BBitmap(BRect(0, 0, size - 1, size - 1), bitmap->ColorSpace());
	
	if (ScaleBitmap(bitmap, scaled, false) == B_OK) {
		delete bitmap;
		bitmap = scaled;
	} else {
		delete scaled;
		scaled = NULL;
	}
	
	return B_OK;
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
ExtendedIcon::ScaleBitmap(BBitmap *source, BBitmap *target, bool border)
{
	if (!source || !target)
		return B_NO_INIT;
	
	rgb_color color;
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
	
	if (border) {
		dst_width -= (src_width >= src_height ? 4 : 3);
		dst_height -= (src_width <= src_height ? 4 : 3);
	}
	
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
	
	if (border) {
		frame.InsetBy(1, 1);
		frame.right -= (src_width >= src_height ? 2 : 1);
		frame.bottom -= (src_width <= src_height ? 2 : 1);
	}
	
	temp->Lock();
	view->DrawBitmapAsync(source, frame);
	
	if (border) {
		frame.InsetBy(-1, -1);
		color.red = BORDER; color.green = BORDER;
		color.blue = BORDER; color.alpha = 0xff;
		view->SetHighColor(color);
		view->StrokeRect(frame);
		
		frame.OffsetBy(1, 1);
		frame.top += 1; frame.left += 1;
		color.red = SHADOW; color.green = SHADOW;
		color.blue = SHADOW; color.alpha = 0xff;
		view->SetHighColor(color);
		view->MovePenTo(frame.RightTop());
		view->StrokeLine(frame.RightBottom());
		view->StrokeLine(frame.LeftBottom());
		
		frame.right += 1; frame.bottom += 1;
		view->MovePenTo(frame.RightTop());
		view->StrokeLine(frame.RightBottom());
		view->StrokeLine(frame.LeftBottom());
	}
	
	view->Sync();
	temp->RemoveChild(view);
	temp->Unlock();
	delete view;
	view = NULL;
	
	memcpy(target->Bits(), temp->Bits(), MIN(target->BitsLength(), temp->BitsLength()));
	delete temp;
	temp = NULL;
	
	return B_OK;
}

status_t
ExtendedIcon::ScaleBilinear(BBitmap *source, BBitmap *target)
{
	BRect bounds = source->Bounds();
	uint32 size = target->Bounds().IntegerWidth();
	uint32 side, width, height, offset_top, offset_right;
	width = (uint32)bounds.Width() + 1;
	uint32 width4 = width * 4;
	height = (uint32)bounds.Height() + 1;
	side = MAX(width, height);
	float step = (float)side / (float)size;
	offset_top = (uint32)((size - (height / step)) / 2);
	offset_right = (uint32)((size - (width / step)) / 2);
	uint8 *source_bits = (uint8 *)source->Bits();
	uint8 *target_bits = (uint8 *)target->Bits();
	
	float fpos = 0.0, fline = 0.0;
	uint32 pos = 0, line = 0;
	uint32 offset_target = 0;
	uint32 line_target = offset_top * size;
	
	uint32 new_bit, new_width = 0;
	
	uint32 h, i, j, k, l;
	uint32 stop_h = size - offset_top;
	uint32 stop_i = size - offset_right;
	uint32 count = (step > 3 ? 3 : (uint32)step);
	
	for (h = offset_top; h < stop_h; h++, line_target += size, fline += step) {
		line = ((uint32)fline) * width * 4;
		if ((uint32)fline + count > height) {
			int32 newcount = height - (uint32)fline;
			if (newcount <= 0)
				break;
			count = (uint32)newcount;
		}
		fpos = 0;
		
		for (i = offset_right; i < stop_i; i++, fpos += step) {
			pos = ((uint32)fpos) * 4 + line;
			if ((uint32)fpos + count > width) {
				int32 newcount = width - (uint32)fpos;
				if (newcount <= 0)
					break;
				count = (uint32)newcount;
			}
			offset_target = (line_target + i) * 4;
			
			for (l = 0; l < 3; l++) {
				new_bit = source_bits[pos + l];
				new_width = 0;
				for (j = 0; j < count; j++, new_width += width4) {
					for (k = 0; k < count; k++) {
						new_bit += source_bits[pos + new_width + l];
						new_bit /= 2;
					}
				}
				target_bits[offset_target + l] = new_bit;
			}
		}
	}
	
	return B_OK;
}

