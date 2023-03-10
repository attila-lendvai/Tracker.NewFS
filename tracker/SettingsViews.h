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

#ifndef _SETTINGS_VIEWS
#define _SETTINGS_VIEWS

#include <Button.h>
#include <CheckBox.h>
#include <RadioButton.h>
#include <TextControl.h>
#include <ColorControl.h>
#include <StringView.h>

#include "Commands.h"
#include "TrackerSettings.h"

const uint32 kSettingsContentsModified = 'Scmo';

class BMenuField;
class BStringView;
class ThemePreView;
class BLinkStringView;

namespace BPrivate {

class SettingsView : public BView {
public:
	SettingsView(BRect, const char *);
	virtual ~SettingsView();

	virtual void SetDefaults();
	virtual void Revert();
	virtual void ShowCurrentSettings(bool sendNotices = false);
	virtual void RecordRevertSettings();
	virtual bool ShowsRevertSettings() const;
protected:
	
	typedef BView _inherited;
};

class DesktopSettingsView : public SettingsView {
public:
	DesktopSettingsView(BRect);
	
	void MessageReceived(BMessage *);
	void AttachedToWindow();
	
	void SetDefaults();
	void Revert();
	void ShowCurrentSettings(bool sendNotices = false);
	void RecordRevertSettings();
	bool ShowsRevertSettings() const;
private:
	BRadioButton *fShowDisksIconRadioButton;
	BRadioButton *fMountVolumesOntoDesktopRadioButton;
	BCheckBox *fMountSharedVolumesOntoDesktopCheckBox;
	BCheckBox *fEjectWhenUnmountingCheckBox;
	BCheckBox *fIntegrateNonBootBeOSDesktopsCheckBox;

	bool fShowDisksIcon;
	bool fMountVolumesOntoDesktop;
	bool fMountSharedVolumesOntoDesktop;
	bool fIntegrateNonBootBeOSDesktops;
	bool fEjectWhenUnmounting;
	
	typedef SettingsView _inherited;
};

class WindowsSettingsView : public SettingsView {
public:
	WindowsSettingsView(BRect);

	void MessageReceived(BMessage *);
	void AttachedToWindow();
	
	void SetDefaults();
	void Revert();
	void ShowCurrentSettings(bool sendNotices = false);
	void RecordRevertSettings();
	bool ShowsRevertSettings() const;
private:
	BCheckBox *fShowFullPathInTitleBarCheckBox;
	BCheckBox *fSingleWindowBrowseCheckBox;
	BCheckBox *fShowNavigatorCheckBox;
	BCheckBox *fShowHomeButtonCheckBox;
	BTextControl *fHomeButtonDirectoryTextControl;
	BCheckBox *fShowSelectionWhenInactiveCheckBox;
	BCheckBox *fSortFolderNamesFirstCheckBox;

	bool fShowFullPathInTitleBar;
	bool fSingleWindowBrowse;
	bool fShowNavigator;
	bool fShowHomeButton;
	char* fHomeButtonDirectory;
	bool fShowSelectionWhenInactive;
	bool fSortFolderNamesFirst;
	
	typedef SettingsView _inherited;
};

class FilePanelSettingsView : public SettingsView {
public:
	FilePanelSettingsView(BRect);
	~FilePanelSettingsView();

	void MessageReceived(BMessage *);
	void AttachedToWindow();
	
	void SetDefaults();
	void Revert();
	void ShowCurrentSettings(bool sendNotices = false);
	void RecordRevertSettings();
	bool ShowsRevertSettings() const;

	void GetAndRefreshDisplayedFigures() const;
private:
	BCheckBox *fDesktopFilePanelRootCheckBox;

	BTextControl *fRecentApplicationsTextControl; // Not used for the moment.
	BTextControl *fRecentDocumentsTextControl;
	BTextControl *fRecentFoldersTextControl;

	bool fDesktopFilePanelRoot;
	int32 fRecentApplications; // Not used for the moment,
	int32 fRecentDocuments;
	int32 fRecentFolders;
	
	mutable int32 fDisplayedAppCount; // Not used for the moment.
	mutable int32 fDisplayedDocCount;
	mutable int32 fDisplayedFolderCount;

	typedef SettingsView _inherited;
};

class TimeFormatSettingsView : public SettingsView {
public:
	TimeFormatSettingsView(BRect);

	void MessageReceived(BMessage *);
	void AttachedToWindow();	

	void SetDefaults();
	void Revert();
	void ShowCurrentSettings(bool sendNotices = false);
	void RecordRevertSettings();
	bool ShowsRevertSettings() const;
	
	void UpdateExamples();
private:
	BRadioButton *f24HrRadioButton;
	BRadioButton *f12HrRadioButton;

	BRadioButton *fYMDRadioButton;
	BRadioButton *fDMYRadioButton;
	BRadioButton *fMDYRadioButton;

	BMenuField *fSeparatorMenuField;
	
	BStringView *fLongDateExampleView;
	BStringView *fShortDateExampleView;
	
	bool f24HrClock;

	FormatSeparator fSeparator;
	DateOrder fFormat;

	typedef SettingsView _inherited;
};

class SpaceBarSettingsView : public SettingsView {
public:
	SpaceBarSettingsView(BRect);
	~SpaceBarSettingsView();

	void MessageReceived(BMessage *);
	void AttachedToWindow();
	
	void SetDefaults();
	void Revert();
	void ShowCurrentSettings(bool sendNotices = false);
	void RecordRevertSettings();
	bool ShowsRevertSettings() const;

private:
	BCheckBox		*fSpaceBarShowCheckBox;
	BColorControl	*fColorControl;
	BMenuField		*fColorPicker;
//	BRadioButton *fUsedRadio;
//	BRadioButton *fWarningRadio;
//	BRadioButton *fFreeRadio;
	int32			fCurrentColor;

	bool			fSpaceBarShow;
	rgb_color		fUsedSpaceColor;
	rgb_color		fFreeSpaceColor;
	rgb_color		fWarningSpaceColor;

	typedef SettingsView _inherited;
};

class TrashSettingsView : public SettingsView {
public:
	TrashSettingsView(BRect);

	void MessageReceived(BMessage *);
	void AttachedToWindow();
	
	void SetDefaults();
	void Revert();
	void ShowCurrentSettings(bool sendNotices = false);
	void RecordRevertSettings();
	bool ShowsRevertSettings() const;

private:
	BCheckBox *fDontMoveFilesToTrashCheckBox;
	BCheckBox *fAskBeforeDeleteFileCheckBox;

	bool fDontMoveFilesToTrash;
	bool fAskBeforeDeleteFile;
	
	typedef SettingsView _inherited;
};

class TransparentSelectionSettingsView : public SettingsView {
public:
	TransparentSelectionSettingsView(BRect);
	~TransparentSelectionSettingsView();

	void MessageReceived(BMessage *);
	void AttachedToWindow();
	
	void SetDefaults();
	void Revert();
	void ShowCurrentSettings(bool sendNotices = false);
	void RecordRevertSettings();
	bool ShowsRevertSettings() const;

private:
	BCheckBox 		*fTransparentSelectionCheckBox;

	BColorControl	*fColorControl;
	BTextControl 	*fTransparentSelectionAlphaTextControl;

	bool 			fTransparentSelection;
	rgb_color		fTransparentSelectionColor;
	int32 			fTransparentSelectionAlpha;

	typedef SettingsView _inherited;
};

class FilteringSettingsView : public SettingsView {
public:
	FilteringSettingsView(BRect);
	~FilteringSettingsView();

	void MessageReceived(BMessage *);

	void AttachedToWindow();
	
	void SetDefaults();
	void Revert();
	void ShowCurrentSettings(bool sendNotices = false);
	void RecordRevertSettings();
	bool ShowsRevertSettings() const;

private:
	BCheckBox 		*fDynamicFilteringCheckBox;
	BCheckBox 		*fDynamicFilteringInvertCheckBox;
	BCheckBox 		*fDynamicFilteringIgnoreCaseCheckBox;
	BMenuField		*fDynamicFilteringExpressionTypeMenuField;

	bool			fDynamicFiltering;
	bool			fDynamicFilteringInvert;
	bool			fDynamicFilteringIgnoreCase;
	int32			fDynamicFilteringExpressionType;

	BCheckBox		*fStaticFilteringCheckBox;
	BListView		*fStaticFilteringListView;
	BScrollView		*fStaticFilteringScrollView;
	BButton			*fStaticFilteringAddFilterButton;
	BButton			*fStaticFilteringRemoveFilterButton;
	
	bool			fStaticFiltering;

	typedef SettingsView _inherited;
};

class UndoSettingsView : public SettingsView {
public:
	UndoSettingsView(BRect);
	~UndoSettingsView();

	void MessageReceived(BMessage *);

	void AttachedToWindow();
	
	void SetDefaults();
	void Revert();
	void ShowCurrentSettings(bool sendNotices = false);
	void RecordRevertSettings();
	bool ShowsRevertSettings() const;
	void GetAndRefreshDisplayFigures() const;

private:
	BCheckBox		*fUndoEnabledCheckBox;
	BTextControl	*fUndoDepthTextControl;
	
	bool			fUndoEnabled;
	int32			fUndoDepth;
	mutable int32	fDisplayedUndoDepth;

	typedef SettingsView _inherited;
};

class IconThemeSettingsView : public SettingsView {

public:
					IconThemeSettingsView(BRect);
					~IconThemeSettingsView();

	void			MessageReceived(BMessage *);
	void			AttachedToWindow();

	void			SetDefaults();
	void			Revert();
	void			ShowCurrentSettings(bool sendNotices = false);
	void			RecordRevertSettings();
	bool			ShowsRevertSettings();
	void			GetAndRefreshDisplayFigures();
	void			UpdatePreview();
	void			UpdateInfo();
	void			UpdateThemeList(bool dowatch = false);

private:
	BOptionPopUp	*fThemesPopUp;
	BButton			*fSequenceOptions;
	BButton			*fApplyTheme;

	BStringView		*fAuthorLabel;
	BStringView		*fAuthorString;
	BStringView		*fCommentLabel;
	BStringView		*fCommentString;
	BStringView		*fLinkLabel;
	BLinkStringView	*fLinkString;

	ThemePreView	*fPreview;

	bool			fThemesEnabled;
	BString			fCurrentTheme;
	BString			fThemeLookupSequence;
	BString			fDisplayedSequence;
	BString			fDisplayedTheme;
	
	typedef SettingsView _inherited;
};

class LanguageThemeSettingsView : public SettingsView {

public:
					LanguageThemeSettingsView(BRect);
					~LanguageThemeSettingsView();

	void			MessageReceived(BMessage *);
	void			AttachedToWindow();

	void			SetDefaults();
	void			Revert();
	void			ShowCurrentSettings(bool sendNotices = false);
	void			RecordRevertSettings();
	bool			ShowsRevertSettings();
	void			GetAndRefreshDisplayFigures();
	void			UpdateInfo();
	void			UpdateThemeList(bool dowatch = false);

private:
	BOptionPopUp	*fThemesPopUp;
	BButton			*fApplyTheme;

	BStringView		*fAuthorLabel;
	BStringView		*fAuthorString;
	BStringView		*fEMailLabel;
	BStringView		*fEMailString;
	BStringView		*fLinkLabel;
	BLinkStringView	*fLinkString;

	BString			fCurrentTheme;
	BString			fDisplayedTheme;

	typedef	SettingsView _inherited;
};

} // namespace BPrivate

using namespace BPrivate;

#endif
