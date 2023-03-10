/*
Open Tracker License

Terms and Conditions

Copyright (c) 1991-2000, Be Incorporated. All rights reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

The above copyright notice and this permission notice applies to all licensees
and shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF TITLE, MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
BE INCORPORATED BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF, OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of Be Incorporated shall not be
used in advertising or otherwise to promote the sale, use or other dealings in
this Software without prior written authorization from Be Incorporated.

Tracker(TM), Be(R), BeOS(R), and BeIA(TM) are trademarks or registered trademarks
of Be Incorporated in the United States and other countries. Other brand product
names are registered trademarks or trademarks of their respective holders.
All rights reserved.
*/

#ifndef	_CONTAINER_WINDOW_H
#define _CONTAINER_WINDOW_H

#include <Window.h>

#include "FavoritesConfig.h"
#include "FavoritesMenu.h"
#include "LockingList.h"
#include "Model.h"
#include "SlowContextPopup.h"
#include "TaskLoop.h"

class BPopUpMenu;
class BMenuBar;

namespace BPrivate {

class BNavigator;
class BPoseView;
class ModelMenuItem;
class AttributeStreamNode;
class BackgroundImage;
class Model;
class ModelNodeLazyOpener;
class SelectionWindow;

#define kDefaultFolderTemplate "DefaultFolderTemplate"

extern const char *kAddOnsMenuName;

const window_feel kPrivateDesktopWindowFeel = window_feel(1024);
const window_look kPrivateDesktopWindowLook = window_look(4);
	// this is a mirror of an app server private values

enum {
	// flags that describe opening of the window
	kRestoreWorkspace = 0x1,
	kIsHidden		  = 0x2
		// set when opening a window during initial Tracker start
};

class BContainerWindow : public BWindow {
public:
	BContainerWindow(LockingList<BWindow> *windowList,
		uint32 containerWindowFlags,
		window_look look = B_DOCUMENT_WINDOW_LOOK,
		window_feel feel = B_NORMAL_WINDOW_FEEL, 
		uint32 flags = B_WILL_ACCEPT_FIRST_CLICK | B_NO_WORKSPACE_ACTIVATION,
		uint32 workspace = B_CURRENT_WORKSPACE);

	virtual ~BContainerWindow();

	virtual void Init(const BMessage *message = NULL);

	static BRect InitialWindowRect(window_feel);
	
	virtual void Minimize(bool minimize);
	virtual void Quit();
	virtual bool QuitRequested();
	
	virtual void UpdateIfTrash(Model *);
	
	virtual	void CreatePoseView(Model *);

	virtual	void ShowContextMenu(BPoint, const entry_ref *, BView *);
	virtual	uint32 ShowDropContextMenu(BPoint);
	virtual	void MenusBeginning();
	virtual	void MenusEnded();
	virtual	void MessageReceived(BMessage *);
	virtual	void FrameResized(float, float);
	virtual	void FrameMoved(BPoint);
	virtual	void Zoom(BPoint, float, float);
	virtual void WorkspacesChanged(uint32, uint32);
	
	// virtuals that control setup of window
	virtual	bool ShouldAddMenus() const;
	virtual	bool ShouldAddScrollBars() const;
	virtual	bool ShouldAddCountView() const;

	virtual	void CheckScreenIntersect();

	bool IsTrash() const;
	bool InTrash() const;
	bool IsPrintersDir() const;
	bool IsDevDir() const;

	virtual bool IsShowing(const node_ref *) const;
	virtual bool IsShowing(const entry_ref *) const;

	void ResizeToFit();

	Model *TargetModel() const;
	BPoseView *PoseView() const;
	BNavigator *Navigator() const;

	virtual	void SelectionChanged();
	virtual void ViewModeChanged(uint32 oldMode, uint32 newMode);
	
	virtual	void RestoreState();
	virtual	void RestoreState(const BMessage &);
	void RestoreStateCommon();
	virtual	void SaveState(bool hide = true);
	virtual	void SaveState(BMessage &) const;
	void UpdateTitle();

	bool StateNeedsSaving() const;
	bool SaveStateIsEnabled() const;
	void SetSaveStateEnabled(bool);
	
	void UpdateBackgroundImage();

	static status_t GetLayoutState(BNode *, BMessage *);
	static status_t SetLayoutState(BNode *, const BMessage *);
		// calls for inheriting window size, attribute layout, etc.
		// deprecated

	virtual	void AddMimeTypesToMenu(BMenu *);
	void AddMimeTypesToMenu();
	virtual	void MarkAttributeMenu(BMenu *);
	void MarkAttributeMenu();
	BMenuItem *NewAttributeMenuItem (const char *label, const char *attrName, int32 attrType,
		float attrWidth, int32 attrAlign, bool attrEditable, bool attrStatField);
	virtual	void NewAttributeMenu(BMenu *);

	void HideAttributeMenu();
	void ShowAttributeMenu();
	PiggybackTaskLoop *DelayedTaskLoop();
		// use for RunLater queueing
	void PulseTaskLoop();
		// called by some view that has pulse, either BackgroundView or BPoseView

	static bool DefaultStateSourceNode(const char *name, BNode *result,
		bool createNew = false, bool createFolder = true);

	// add-on iteration
	void EachAddon(bool(*)(const Model *, const char *, uint32 shortcut, bool primary, void *), void *);
	BPopUpMenu *ContextMenu();
	uint32 ModifiersAtContextPopup() const;
	
	// drag&drop support
	status_t DragStart(const BMessage *);
	void DragStop();
	bool Dragging() const;
	BMessage *DragMessage() const;

	void ShowSelectionWindow();
	
	void ShowNavigator(bool);
	void SetSingleWindowBrowseShortcuts(bool);
	
	void SetPathWatchingEnabled(bool);
	bool IsPathWatchingEnabled(void) const;
	
	void SetIsAboutToBeActivated(bool);
	bool IsAboutToBeActivated() const;
	
protected:
	virtual BPoseView *NewPoseView(Model *, BRect, uint32);
		// instantiate a different flavor of BPoseView for different
		// ContainerWindows

	virtual void RestoreWindowState(AttributeStreamNode *);
	virtual void RestoreWindowState(const BMessage &);
	virtual void SaveWindowState(AttributeStreamNode *);
	virtual void SaveWindowState(BMessage &) const;
	
	virtual bool NeedsDefaultStateSetup();
	virtual void SetUpDefaultState();
		// these two virtuals control setting up a new folder that
		// does not have any state settings yet with the default

	enum UpdateMenuContext {
		kMenuBarContext,
		kPosePopUpContext,
		kWindowPopUpContext
	};
	
	virtual	void AddMenus();
	virtual void AddShortcuts();
		// add equivalents of the menu shortcuts to the menuless desktop window
	virtual	void AddFileMenu(BMenu *menu);
	virtual void AddEditMenu(BMenu *menu);
	virtual	void AddWindowMenu(BMenu *menu);
	
	virtual void AddContextMenus();

	virtual	void AddFileContextMenus(BMenu *);
			void AddFileContextMenus(BMenu *, bool);
	virtual void AddEditContextMenus(BMenu *);
			void AddEditContextMenus(BMenu *, UpdateMenuContext);
	virtual	void AddWindowContextMenus(BMenu *);
			void AddWindowContextMenus(BMenu *, bool);
	virtual void AddContextSensitive(BMenu *);
	virtual	void AddVolumeContextMenus(BMenu *);
	virtual	void AddDropContextMenus(BMenu *);
	virtual void AddTrashContextMenu();
	
	virtual void RepopulateMenus();
	
	virtual	void SetCutItem(BMenu *);
	virtual	void SetCopyItem(BMenu *);
	virtual	void SetPasteItem(BMenu *);
	virtual	void SetCleanUpItem(BMenu *);
	virtual void SetCloseItem(BMenu *);
	virtual	void SetupNavigationMenu(const entry_ref *, BMenu *);
	virtual	void SetupMoveCopyMenus(const entry_ref *, BMenu *);
	virtual	void PopulateMoveCopyNavMenu(BNavMenu *, uint32, const entry_ref *, bool);

	virtual	void SetupOpenWithMenu(BMenu *);
	virtual	void SetUpEditQueryItem(BMenu *);
	virtual	void SetUpDiskMenu(BMenu *);

	virtual	void BuildAddOnMenu(BMenu *);

	virtual void UpdateMenu(BMenu *menu, UpdateMenuContext context);
	
	BHandler *ResolveSpecifier(BMessage *, int32, BMessage *, int32,
		const char *);

	bool EachAddon(BPath &path, bool(*)(const Model *, const char *, uint32, bool, void *),
		BObjectList<Model> *, void *);
	void LoadAddOn(BMessage *);

	BPopUpMenu *fFileContextMenu;
	BPopUpMenu *fEditContextMenu;
	BPopUpMenu *fWindowContextMenu;
	BPopUpMenu *fDropContextMenu;
	BPopUpMenu *fVolumeContextMenu;
	BPopUpMenu *fTrashContextMenu;
	BSlowContextMenu *fDragContextMenu;
	BMenuItem *fMoveToItem;
	BMenuItem *fCopyToItem;
	BMenuItem *fCreateLinkItem;
	BMenuItem *fOpenWithItem;
	BMenuItem *fUndoItem;
	BMenuItem *fRedoItem;
	ModelMenuItem *fNavigationItem;
	BMenuBar *fMenuBar;
	BNavigator *fNavigator;
	BPoseView *fPoseView;
	LockingList<BWindow> *fWindowList;
	BMenu *fAttrMenu;
	BMenu *fWindowMenu;
	BMenu *fEditMenu;
	BMenu *fFileMenu;
	FavoritesMenu *fFavoritesMenu;

	SelectionWindow *fSelectionWindow;

	PiggybackTaskLoop *fTaskLoop;

	bool fIsTrash;
	bool fInTrash;
	bool fIsPrinters;
	bool fIsDevDir;

	uint32 fContainerWindowFlags;
	BackgroundImage *fBackgroundImage;

private:
	BRect fSavedZoomRect;
	
	static BRect fNewWindRect;

	BPopUpMenu *fContextMenu;
	BMessage *fDragMessage;
	BObjectList<BString> *fCachedTypesList;
	bool fWaitingForRefs;

	bool fStateNeedsSaving;
	bool fSaveStateIsEnabled;
	
	bool fIsWatchingPath;
	bool fIsAboutToBeActivated;

	TFavoritesConfigWindow *fConfigWindow;
	
	uint32 fModifiersAtContextPopup;

	typedef BWindow _inherited;

friend int32 show_context_menu(void*);
friend class BackgroundView;
};

class WindowStateNodeOpener {
	// this class manages opening and closing the proper node for
	// state restoring / saving; the constructor knows how to decide wether
	// to use a special directory for root, etc.
	// setter calls used when no attributes can be read from a node and defaults
	// are to be substituted
public:
	WindowStateNodeOpener(BContainerWindow *window, bool forWriting);
	virtual ~WindowStateNodeOpener();
	
	void SetTo(const BDirectory *);
	void SetTo(const BEntry *entry, bool forWriting);
	void SetTo(Model *, bool forWriting);

	AttributeStreamNode *StreamNode() const;
	BNode *Node() const;

private:
	ModelNodeLazyOpener *fModelOpener;
	BNode *fNode;
	AttributeStreamNode *fStreamNode;
};

class BackgroundView : public BView {
	// background view placed in a BContainerWindow, under the pose view
public:
	BackgroundView(BRect);
	virtual	void AttachedToWindow();
	virtual	void FrameResized(float, float);
	virtual	void Draw(BRect);

	void PoseViewFocused(bool);
	virtual void Pulse();

protected:
	virtual void WindowActivated(bool);

private:
	typedef BView _inherited;
};

int CompareLabels(const BMenuItem *, const BMenuItem *);

// inlines ---------

inline BNavigator * 
BContainerWindow::Navigator() const
{
	return fNavigator;
}

inline BPoseView *
BContainerWindow::PoseView() const
{
	return fPoseView;
}

inline bool
BContainerWindow::IsTrash() const
{
	return fIsTrash;
}

inline bool
BContainerWindow::InTrash() const
{
	return fInTrash;
}

inline bool 
BContainerWindow::IsPrintersDir() const
{
	return fIsPrinters;
}

inline bool
BContainerWindow::IsDevDir() const
{
	return fIsDevDir;
}

inline void 
BContainerWindow::SetUpDiskMenu(BMenu *)
{
	// nothing at this level
}

inline BPopUpMenu *
BContainerWindow::ContextMenu()
{
	return fContextMenu;
}

inline bool 
BContainerWindow::Dragging() const
{
	return fDragMessage && fCachedTypesList;
}

inline BMessage *
BContainerWindow::DragMessage() const
{
	return fDragMessage;
}
inline
bool
BContainerWindow::SaveStateIsEnabled() const
{
	return fSaveStateIsEnabled;
}

inline 
void
BContainerWindow::SetSaveStateEnabled(bool value)
{
	fSaveStateIsEnabled = value;
}

inline
bool
BContainerWindow::IsPathWatchingEnabled() const
{
	return fIsWatchingPath;
}

inline
uint32
BContainerWindow::ModifiersAtContextPopup() const
{
	return fModifiersAtContextPopup;
}

inline
void
BContainerWindow::SetIsAboutToBeActivated(bool enabled)
{
	fIsAboutToBeActivated = enabled;
}

inline
bool
BContainerWindow::IsAboutToBeActivated() const
{
	return fIsAboutToBeActivated;
}

filter_result ActivateWindowFilter(BMessage *message, BHandler **target,
	BMessageFilter *messageFilter);

} // namespace BPrivate

using namespace BPrivate;

#endif
