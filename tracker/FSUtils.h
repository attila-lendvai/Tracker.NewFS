#if !defined(_FSUTILS_H)
#define _FSUTILS_H

// Open Tracker License
//
// Terms and Conditions
//
// Copyright (c) 1991-2000, Be Incorporated. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of
// this software and associated documentation files (the "Software"), to deal in
// the Software without restriction, including without limitation the rights to
// use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
// of the Software, and to permit persons to whom the Software is furnished to do
// so, subject to the following conditions:
// 
// The above copyright notice and this permission notice applies to all licensees
// and shall be included in all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF TITLE, MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
// BE INCORPORATED BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
// AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF, OR IN CONNECTION
// WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//
// Except as contained in this notice, the name of Be Incorporated shall not be
// used in advertising or otherwise to promote the sale, use or other dealings in
// this Software without prior written authorization from Be Incorporated.
// 
// Tracker(TM), Be(R), BeOS(R), and BeIA(TM) are trademarks or registered trademarks
// of Be Incorporated in the United States and other countries. Other brand product
// names are registered trademarks or trademarks of their respective holders.
// All rights reserved.

#define B_DESKTOP_DIR_NAME "Desktop"

#include <Node.h>
#include <FindDirectory.h>
#include <fs_attr.h>

#include <vector>

namespace BPrivate {

enum ReadAttrResult {
	kReadAttrFailed,
	kReadAttrNativeOK,
	kReadAttrForeignOK
};

ReadAttrResult ReadAttr(const BNode &, const char *hostAttrName, const char *foreignAttrName,
	type_code , off_t , void *, size_t , void (*swapFunc)(void *) = 0,
	bool isForeign = false);
	// Endian swapping ReadAttr call; endianness is determined by trying first the
	// native attribute name, then the foreign one; an endian swapping function can
	// be passed, if null data won't be swapped; if <isForeign> set the foreign endianness
	// will be read directly without first trying the native one
	
ReadAttrResult GetAttrInfo(const BNode &, const char *hostAttrName, const char *foreignAttrName,
	type_code * = NULL, size_t * = NULL);


status_t TrackerLaunch(const entry_ref *app, bool async);
status_t TrackerLaunch(const BMessage *refs, bool async, bool okToRunOpenWith = true);
status_t TrackerLaunch(const entry_ref *app, const BMessage *refs, bool async,
	bool okToRunOpenWith = true);
status_t LaunchBrokenLink(const char *, const BMessage *);

// Deprecated calls use newer calls above instead

_IMPEXP_TRACKER void FSLaunchItem(const entry_ref *, BMessage * = NULL, int32 workspace = -1);
_IMPEXP_TRACKER status_t FSLaunchItem(const entry_ref *, BMessage *,
	int32 workspace, bool asynch);
_IMPEXP_TRACKER void FSOpenWithDocuments(const entry_ref *executableToLaunch,
	BMessage *documentEntryRefs);
_IMPEXP_TRACKER status_t FSLaunchUsing(const entry_ref *ref, BMessage *listOfRefs);



// some extra directory_which values
// move these to FindDirectory.h
const uint32 B_USER_MAIL_DIRECTORY = 3500;
const uint32 B_USER_QUERIES_DIRECTORY = 3501;
const uint32 B_USER_PEOPLE_DIRECTORY = 3502;
const uint32 B_USER_DOWNLOADS_DIRECTORY = 3503;
const uint32 B_USER_DESKBAR_APPS_DIRECTORY = 3504;
const uint32 B_USER_DESKBAR_PREFERENCES_DIRECTORY = 3505;
const uint32 B_USER_DESKBAR_DEVELOP_DIRECTORY = 3506;

const int32 B_BOOT_DISK = 10000000;
	// map /boot into the directory_which enum for convenience

class WellKnowEntryList {
	// matches up names, id's and node_refs of well known entries in the
	// system hierarchy

public:
	struct WellKnownEntry {
		WellKnownEntry(const node_ref *node, directory_which which, const char *name)
			:	node(*node),
				which(which),
				name(name)
			{}

		// mwcc needs these explicitly to use vector
		WellKnownEntry(const WellKnownEntry &clone)
			:	node(clone.node),
				which(clone.which),
				name(clone.name)
			{}
		
		WellKnownEntry()
			{}
		
		node_ref node;
		directory_which which;
		const char *name;
	};
	
	static directory_which Match(const node_ref *);
	static const WellKnownEntry *MatchEntry(const node_ref *);
	static void Quit();

private:
	const WellKnownEntry *MatchEntryCommon(const node_ref *);
	WellKnowEntryList();
	void AddOne(directory_which, const char *name);
	void AddOne(directory_which, const char *path, const char *name);
	void AddOne(directory_which, directory_which base, const char *extension,
		const char *name);
	
	vector<WellKnownEntry> entries;
	static WellKnowEntryList *self;
};

bool ConfirmChangeIfWellKnownDirectory(const BEntry *entry, const char *action,
	bool dontAsk = false, int32 *confirmedAlready = NULL);


}	// namespace BPrivate

#endif // _FSUTILS_H
