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

#include "FSUtils.h"
#include "Attributes.h"
#include "ThreadMagic.h"
#include "OverrideAlert.h"
#include "Tracker.h"

#include <Roster.h>
#include <Alert.h>
#include <Message.h>
#include <NodeInfo.h>

namespace BPrivate {

WellKnowEntryList *	WellKnowEntryList :: self = 0;

const char *kFindAlternativeStr = "Would you like to find some other suitable application?";
const char *kFindApplicationStr = "Would you like to find a suitable application to "
	"open the file?";


ReadAttrResult
ReadAttr(const BNode &node, const char *hostAttrName, const char *foreignAttrName,
	type_code type, off_t offset, void *buffer, size_t length,
	void (*swapFunc)(void *), bool isForeign) {
	
	if (!isForeign && node.ReadAttr(hostAttrName, type, offset, buffer, length) == (ssize_t)length)
		return kReadAttrNativeOK;

	// PRINT(("trying %s\n", foreignAttrName));
	// try the other endianness	
	if (node.ReadAttr(foreignAttrName, type, offset, buffer, length) != (ssize_t)length)
		return kReadAttrFailed;
	
	// PRINT(("got %s\n", foreignAttrName));
	if (swapFunc)
		(swapFunc)(buffer);		// run the endian swapper

	return kReadAttrForeignOK;
}

ReadAttrResult 
GetAttrInfo(const BNode &node, const char *hostAttrName, const char *foreignAttrName,
	type_code *type , size_t *size) {

	attr_info info;
	
	if (node.GetAttrInfo(hostAttrName, &info) == B_OK) {
		if (type)
			*type = info.type;
		if (size)
			*size = (size_t)info.size;

		return kReadAttrNativeOK;
	}

	if (node.GetAttrInfo(foreignAttrName, &info) == B_OK) {
		if (type)
			*type = info.type;
		if (size)
			*size = (size_t)info.size;

		return kReadAttrForeignOK;
	}
	return kReadAttrFailed;
}

// launching code

static status_t
TrackerOpenWith(const BMessage *refs)
{
	BMessage clone(*refs);
	ASSERT(dynamic_cast<TTracker *>(be_app));
	ASSERT(clone.what);
	clone.AddInt32("launchUsingSelector", 0);
	// runs the Open With window
	be_app->PostMessage(&clone);

	return B_OK;
}

static void 
AsynchLaunchBinder(void (*func)(const entry_ref &, const BMessage &, bool on),
	const entry_ref *entry, const BMessage *message, bool on)
{
	entry_ref fake_entry;
	BMessage fake_msg;

	LaunchInNewThread("LaunchTask", B_NORMAL_PRIORITY, func,
		(entry) ? *entry : fake_entry, (message) ? *message : fake_msg, on);
}

static bool
SniffIfGeneric(const entry_ref *ref)
{
	BNode node(ref);
	char type[B_MIME_TYPE_LENGTH];
	BNodeInfo info(&node);
	if (info.GetType(type) == B_OK && strcasecmp(type, B_FILE_MIME_TYPE) != 0)
		// already has a type and it's not octet stream
		return false;
	
	BPath path(ref);
	if (path.Path()) {
		// force a mimeset
		node.RemoveAttr(kAttrMIMEType);
		update_mime_info(path.Path(), 0, 1, 1);
	}
	
	return true;
}

static void
SniffIfGeneric(const BMessage *refs)
{
	entry_ref ref;
	for (int32 index = 0; ; index++) {
		if (refs->FindRef("refs", index, &ref) != B_OK)
			break;
		SniffIfGeneric(&ref);
	}
}

static void
_TrackerLaunchAppWithDocuments(const entry_ref &appRef, const BMessage &refs, bool openWithOK)
{
	team_id team;

	status_t error = B_ERROR;
	BString alertString;

	for (int32 mimesetIt = 0; ; mimesetIt++) {
		error = be_roster->Launch(&appRef, &refs, &team);
		if (error == B_ALREADY_RUNNING)
			// app already running, not really an error
			error = B_OK;
		
		if (error == B_OK)
			break;

		if (mimesetIt > 0)
			break;
		
		// failed to open, try mimesetting the refs and launching again
		SniffIfGeneric(&refs);
	}
	
	if (error == B_OK) {
		// close possible parent window, if specified
		node_ref *nodeToClose = 0;
		int32 numBytes;
		refs.FindData("nodeRefsToClose", B_RAW_TYPE, (const void **)&nodeToClose, &numBytes);
		if (nodeToClose)
			dynamic_cast<TTracker *>(be_app)->CloseParent(*nodeToClose);
	} else {
		alertString << "Could not open \"" << appRef.name << "\" (" << strerror(error) << "). ";
		if (refs.what != 12345678 && openWithOK) {
			alertString << kFindAlternativeStr;
			if ((new BAlert("", alertString.String(), "Cancel", "Find", 0,
					B_WIDTH_AS_USUAL, B_WARNING_ALERT))->Go() == 1)
				error = TrackerOpenWith(&refs);
		} else
			(new BAlert("", alertString.String(), "Cancel", 0, 0,
				B_WIDTH_AS_USUAL, B_WARNING_ALERT))->Go();
	}
}

extern "C" char** environ;
extern "C" _IMPEXP_ROOT status_t _kload_image_etc_(int argc, char **argv, char **envp,
	char *buf, int bufsize);


static status_t
LoaderErrorDetails(const entry_ref *app, BString &details)
{
	BPath path;
	BEntry appEntry(app, true);
	status_t result = appEntry.GetPath(&path);
	
	if (result != B_OK)
		return result;
	
	char *argv[2] = { const_cast<char *>(path.Path()), 0};

	result = _kload_image_etc_(1, argv, environ, details.LockBuffer(1024), 1024);
	details.UnlockBuffer();

	return B_OK;
}

static void
_TrackerLaunchDocuments(const entry_ref &/*doNotUse*/, const BMessage &refs,
	bool openWithOK)
{
	BMessage copyOfRefs(refs);
	
	entry_ref documentRef;
	if (copyOfRefs.FindRef("refs", &documentRef) != B_OK)
		// nothing to launch, we are done
		return;

	status_t error = B_ERROR;
	entry_ref app;
	BMessage *refsToPass = NULL;
	BString alertString;
	const char *alternative = 0;

	for (int32 mimesetIt = 0; ; mimesetIt++) {
		alertString = "";
		error = be_roster->FindApp(&documentRef, &app);
			
		if (error != B_OK && mimesetIt == 0) {
			SniffIfGeneric(&copyOfRefs);
			continue;
		}
	
		
		if (error != B_OK) {
			alertString << "Could not find an application to open \"" << documentRef.name
				<< "\" (" << strerror(error) << "). ";
			if (openWithOK)
				alternative = kFindApplicationStr;

			break;
		} else {
	
			BEntry appEntry(&app, true);
			for (int32 index = 0;;) {
				// remove the app itself from the refs received so we don't try
				// to open ourselves
				entry_ref ref;
				if (copyOfRefs.FindRef("refs", index, &ref) != B_OK)
					break;
				
				// deal with symlinks properly
				BEntry documentEntry(&ref, true);
				if (appEntry == documentEntry) {
					PRINT(("stripping %s, app %s \n", ref.name, app.name));
					copyOfRefs.RemoveData("refs", index);
				} else {
					PRINT(("leaving %s, app %s  \n", ref.name, app.name));
					index++;
				}
			}
	
			refsToPass = CountRefs(&copyOfRefs) > 0 ? &copyOfRefs: 0;
			team_id team;
			error = be_roster->Launch(&app, refsToPass, &team);
			if (error == B_ALREADY_RUNNING) 
				// app already running, not really an error
				error = B_OK;
			if (error == B_OK || mimesetIt != 0)
				break;
			
			SniffIfGeneric(&copyOfRefs);
		}
	}
	
	if (error != B_OK && alertString.Length() == 0) {
		BString loaderErrorString;
		bool openedDocuments = true;
	
		if (!refsToPass) {
			// we just double clicked the app itself, do not offer to
			// find a handling app
			openWithOK = false;
			openedDocuments = false;
		}

		if (error == B_LAUNCH_FAILED_EXECUTABLE && !refsToPass) {
			alertString << "Could not open \"" << app.name
				<< "\". The file is mistakenly marked as executable. ";
			alternative = kFindApplicationStr;
		} else if (error == B_LAUNCH_FAILED_APP_IN_TRASH) {
			alertString << "Could not open \"" << documentRef.name
				<< "\" because application \"" << app.name << "\" is in the trash. ";
			alternative = kFindAlternativeStr;
		} else if (error == B_LAUNCH_FAILED_APP_NOT_FOUND) {
			alertString << "Could not open \"" << documentRef.name << "\" "
				<< "(" << strerror(error) << "). ";
			alternative = kFindAlternativeStr;
		} else if (error == B_MISSING_SYMBOL
			&& LoaderErrorDetails(&app, loaderErrorString) == B_OK) {
			alertString << "Could not open \"" << documentRef.name << "\" ";
			if (openedDocuments)
				alertString << "with application \"" << app.name << "\" ";
			alertString << "(Missing symbol: " << loaderErrorString << "). \n";
			alternative = kFindAlternativeStr;
		} else if (error == B_MISSING_LIBRARY
			&& LoaderErrorDetails(&app, loaderErrorString) == B_OK) {
			alertString << "Could not open \"" << documentRef.name << "\" ";
			if (openedDocuments)
				alertString << "with application \"" << app.name << "\" ";
			alertString << "(Missing library: " << loaderErrorString << "). \n";
			alternative = kFindAlternativeStr;
		} else {
			alertString << "Could not open \"" << documentRef.name
				<< "\" with application \"" << app.name << "\" (" << strerror(error) << "). ";
			alternative = kFindAlternativeStr;
		}
	}

	if (error != B_OK) {
		if (openWithOK) {
			ASSERT(alternative);
			alertString << alternative;
			if ((new BAlert("", alertString.String(), "Cancel", "Find", 0,
					B_WIDTH_AS_USUAL, B_WARNING_ALERT))->Go() == 1)
				error = TrackerOpenWith(&refs);
		} else 
			(new BAlert("", alertString.String(), "Cancel", 0, 0,
					B_WIDTH_AS_USUAL, B_WARNING_ALERT))->Go();
	}
}

// the following three calls don't return any reasonable error codes,
// should fix that, making them void

status_t 
TrackerLaunch(const entry_ref *appRef, const BMessage *refs, bool async, bool openWithOK)
{
	if (!async)
		_TrackerLaunchAppWithDocuments(*appRef, *refs, openWithOK);
	else
		AsynchLaunchBinder(&_TrackerLaunchAppWithDocuments, appRef, refs, openWithOK);

	return B_OK;
}

status_t 
TrackerLaunch(const entry_ref *appRef, bool async)
{
	BMessage msg(12345678);
	
	if (!async)
		_TrackerLaunchAppWithDocuments(*appRef, msg, false);
	else
		AsynchLaunchBinder(&_TrackerLaunchAppWithDocuments, appRef, &msg, false);

	return B_OK;
}

status_t 
TrackerLaunch(const BMessage *refs, bool async, bool openWithOK)
{
	entry_ref fake;
	if (!async) 
		_TrackerLaunchDocuments(fake, *refs, openWithOK);
	else
		AsynchLaunchBinder(&_TrackerLaunchDocuments, &fake, refs, openWithOK);

	return B_OK;
}

status_t
LaunchBrokenLink(const char *signature, const BMessage *refs)
{
	// This call is to support a hacky workaround for double-clicking
	// broken refs for cifs
	be_roster->Launch(signature, const_cast<BMessage *>(refs));
	return B_OK;
}

// external launch calls; need to be robust, work if Tracker is not running

_IMPEXP_TRACKER status_t 
FSLaunchItem(const entry_ref *application, const BMessage *refsReceived,
	bool async, bool openWithOK)
{
	return TrackerLaunch(application, refsReceived, async, openWithOK);
}


_IMPEXP_TRACKER status_t 
FSOpenWith(BMessage *listOfRefs)
{
	status_t result = B_ERROR;
	listOfRefs->what = B_REFS_RECEIVED;
	
	if (dynamic_cast<TTracker *>(be_app)) 
		result = TrackerOpenWith(listOfRefs);
	else 
		ASSERT(!"not yet implemented");

	return result;
}

// legacy calls, need for compatibility

void 
FSOpenWithDocuments(const entry_ref *executable, BMessage *documents)
{
	TrackerLaunch(executable, documents, true);
	delete documents;
}

status_t
FSLaunchUsing(const entry_ref *ref, BMessage *listOfRefs)
{
	BMessage temp(B_REFS_RECEIVED);
	if (!listOfRefs) {
		ASSERT(ref);
		temp.AddRef("refs", ref);
		listOfRefs = &temp;
	}
	FSOpenWith(listOfRefs);
	return B_OK;
}

status_t
FSLaunchItem(const entry_ref *ref, BMessage* message, int32, bool async)
{
	if (message) 
		message->what = B_REFS_RECEIVED;

	status_t result = TrackerLaunch(ref, message, async, true);
	delete message;
	return result;
}


void
FSLaunchItem(const entry_ref *ref, BMessage *message, int32 workspace)
{
	FSLaunchItem(ref, message, workspace, true);
}

directory_which 
WellKnowEntryList::Match(const node_ref *node)
{
	const WellKnownEntry *result = MatchEntry(node);
	if (result)
		return result->which;
	
	return (directory_which)-1;
}

const WellKnowEntryList::WellKnownEntry *
WellKnowEntryList::MatchEntry(const node_ref *node)
{
	if (!self)
		self = new WellKnowEntryList();
	
	return self->MatchEntryCommon(node);
}

const WellKnowEntryList::WellKnownEntry *
WellKnowEntryList::MatchEntryCommon(const node_ref *node)
{	
	uint32 count = entries.size();
	for (uint32 index = 0; index < count; index++)
		if (*node == entries[index].node)
			return &entries[index];
	
	return NULL;
}


void
WellKnowEntryList::Quit()
{
	delete self;
	self = NULL;
}

void
WellKnowEntryList::AddOne(directory_which which, const char *name)
{
	BPath path;
	if (find_directory(which, &path, true) != B_OK)
		return;
	
	BEntry entry(path.Path());
	node_ref node;
	if (entry.GetNodeRef(&node) != B_OK)
		return;
	
	entries.push_back(WellKnownEntry(&node, which, name));
}

void 
WellKnowEntryList::AddOne(directory_which which, directory_which base,
	const char *extra, const char *name)
{
	BPath path;
	if (find_directory(base, &path, true) != B_OK)
		return;
	
	path.Append(extra);
	BEntry entry(path.Path());
	node_ref node;
	if (entry.GetNodeRef(&node) != B_OK)
		return;
	
	entries.push_back(WellKnownEntry(&node, which, name));
}

void 
WellKnowEntryList::AddOne(directory_which which, const char *path, const char *name)
{
	BEntry entry(path);
	node_ref node;
	if (entry.GetNodeRef(&node) != B_OK)
		return;
	
	entries.push_back(WellKnownEntry(&node, which, name));
}


WellKnowEntryList::WellKnowEntryList()
{
	AddOne(B_BEOS_DIRECTORY, "beos");
	AddOne((directory_which)B_BOOT_DISK, "/boot", "boot");
	AddOne(B_USER_DIRECTORY, "home");
	AddOne(B_BEOS_SYSTEM_DIRECTORY, "system");
	
	AddOne(B_BEOS_FONTS_DIRECTORY, "fonts");
	AddOne(B_COMMON_FONTS_DIRECTORY, "fonts");
	AddOne(B_USER_FONTS_DIRECTORY, "fonts");
	
	AddOne(B_BEOS_APPS_DIRECTORY, "apps");
	AddOne(B_APPS_DIRECTORY, "apps");
	AddOne((directory_which)B_USER_DESKBAR_APPS_DIRECTORY, B_USER_DESKBAR_DIRECTORY,
		"Applications", "apps");

	AddOne(B_BEOS_PREFERENCES_DIRECTORY, "preferences");
	AddOne(B_PREFERENCES_DIRECTORY, "preferences");
	AddOne((directory_which)B_USER_DESKBAR_PREFERENCES_DIRECTORY, B_USER_DESKBAR_DIRECTORY,
		"Preferences", "preferences");

	AddOne((directory_which)B_USER_MAIL_DIRECTORY, B_USER_DIRECTORY, "mail", "mail");
	
	AddOne((directory_which)B_USER_QUERIES_DIRECTORY, B_USER_DIRECTORY, "queries", "queries");


	
	AddOne(B_COMMON_DEVELOP_DIRECTORY, "develop");
	AddOne((directory_which)B_USER_DESKBAR_DEVELOP_DIRECTORY, B_USER_DESKBAR_DIRECTORY,
		"Development", "develop");
	
	AddOne(B_USER_CONFIG_DIRECTORY, "config");
	
	AddOne((directory_which)B_USER_PEOPLE_DIRECTORY, B_USER_DIRECTORY, "people", "people");
	
	AddOne((directory_which)B_USER_DOWNLOADS_DIRECTORY, B_USER_DIRECTORY, "Downloads",
		"Downloads");
}


bool 
DirectoryMatchesOrContains(const BEntry *entry, directory_which which)
{
	BPath path;
	if (find_directory(which, &path, false, NULL) != B_OK)
		return false;
	
	BEntry dirEntry(path.Path());
	if (dirEntry.InitCheck() != B_OK)
		return false;
	
	if (dirEntry == *entry)
		// root level match
		return true;
	
	BDirectory dir(&dirEntry);
	return dir.Contains(entry);
}

bool 
DirectoryMatchesOrContains(const BEntry *entry, const char *additionalPath,
	directory_which which)
{
	BPath path;
	if (find_directory(which, &path, false, NULL) != B_OK)
		return false;
	
	path.Append(additionalPath);
	BEntry dirEntry(path.Path());
	if (dirEntry.InitCheck() != B_OK)
		return false;
	
	if (dirEntry == *entry)
		// root level match
		return true;
	
	BDirectory dir(&dirEntry);
	return dir.Contains(entry);
}


bool 
DirectoryMatches(const BEntry *entry, directory_which which)
{
	BPath path;
	if (find_directory(which, &path, false, NULL) != B_OK)
		return false;
	
	BEntry dirEntry(path.Path());
	if (dirEntry.InitCheck() != B_OK)
		return false;
	
	return dirEntry == *entry;
}

bool 
DirectoryMatches(const BEntry *entry, const char *additionalPath, directory_which which)
{
	BPath path;
	if (find_directory(which, &path, false, NULL) != B_OK)
		return false;
	
	path.Append(additionalPath);
	BEntry dirEntry(path.Path());
	if (dirEntry.InitCheck() != B_OK)
		return false;
	
	return dirEntry == *entry;
}


enum {
	kNotConfirmed,
	kConfirmedHomeMove,
	kConfirmedAll
};

bool
ConfirmChangeIfWellKnownDirectory(const BEntry *entry, const char *action,
	bool dontAsk, int32 *confirmedAlready)
{
	// Don't let the user casually move/change important files/folders
	//
	// This is a cheap replacement for having a real UID support turned
	// on and not running as root all the time

	if (confirmedAlready && *confirmedAlready == kConfirmedAll)
		return true;

	if (!DirectoryMatchesOrContains(entry, B_BEOS_DIRECTORY)
		&& !DirectoryMatchesOrContains(entry, B_USER_DIRECTORY))
		// quick way out
		return true;

	const char *warning = NULL;
	bool requireOverride = true;

	if (DirectoryMatches(entry, B_BEOS_DIRECTORY))
		warning = "If you %s the beos folder, you won't be able to "
			"boot BeOS! Are you sure you want to do this? To %s the folder "
			"anyway, hold down the Shift key and click \"Do it\".";
	else if (DirectoryMatchesOrContains(entry, B_BEOS_SYSTEM_DIRECTORY))
		warning = "If you %s the system folder or its contents, you "
			"won't be able to boot BeOS! Are you sure you want to do this? "
			"To %s the system folder or its contents anyway, hold down "
			"the Shift key and click \"Do it\".";
	else if (DirectoryMatches(entry, B_USER_DIRECTORY)) {
		warning = "If you %s the home folder, BeOS may not "
			"behave properly! Are you sure you want to do this? "
			"To %s the home anyway, click \"Do it\".";
		requireOverride = false;
	} else if (DirectoryMatchesOrContains(entry, B_USER_CONFIG_DIRECTORY)
		|| DirectoryMatchesOrContains(entry, B_COMMON_SETTINGS_DIRECTORY)) {
		
		if (DirectoryMatchesOrContains(entry, "beos_mime", B_USER_SETTINGS_DIRECTORY)
			|| DirectoryMatchesOrContains(entry, "beos_mime", B_COMMON_SETTINGS_DIRECTORY)) {
			warning = "If you %s the mime settings, BeOS may not "
				"behave properly! Are you sure you want to do this? "
				"To %s the mime settings anyway, click \"Do it\".";
			requireOverride = false;
		} else if (DirectoryMatches(entry, B_USER_CONFIG_DIRECTORY)) {
			warning = "If you %s the config folder, BeOS may not "
				"behave properly! Are you sure you want to do this? "
				"To %s the config folder anyway, click \"Do it\".";
			requireOverride = false;
		} else if (DirectoryMatches(entry, B_USER_SETTINGS_DIRECTORY)
			|| DirectoryMatches(entry, B_COMMON_SETTINGS_DIRECTORY)) {
			warning = "If you %s the settings folder, BeOS may not "
				"behave properly! Are you sure you want to do this? "
				"To %s the settings folder anyway, click \"Do it\".";
			requireOverride = false;
		}
	}
	
	if (!warning)
		return true;

	if (dontAsk)
		return false;

	if (confirmedAlready && *confirmedAlready == kConfirmedHomeMove
		&& !requireOverride)
		// we already warned about moving home this time around
		return true;

	char buffer[256];
	sprintf(buffer, warning, action, action);

	if ((new OverrideAlert("", buffer, "Do it", (requireOverride ? B_SHIFT_KEY : 0),
		"Cancel", 0, NULL, 0, B_WIDTH_AS_USUAL, B_WARNING_ALERT))->Go() == 1) {
		if (confirmedAlready)
			*confirmedAlready = kNotConfirmed;
		return false;
	}
	
	if (confirmedAlready) {
		if (!requireOverride)
			*confirmedAlready = kConfirmedHomeMove;
		else
			*confirmedAlready = kConfirmedAll;
	}

	return true;
}

}	// namespace BPrivate