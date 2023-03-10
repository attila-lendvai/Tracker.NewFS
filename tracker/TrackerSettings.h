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

#ifndef	_TRACKER_SETTINGS_H
#define	_TRACKER_SETTINGS_H

#include "Utilities.h"
#include "Settings.h"

namespace BPrivate {

enum FormatSeparator {
		kNoSeparator,
		kSpaceSeparator,
		kMinusSeparator,
		kSlashSeparator,
		kBackslashSeparator,
		kDotSeparator,
		kSeparatorsEnd
};
	
enum DateOrder {
		kYMDFormat,
		kDMYFormat,
		kMDYFormat,
		kDateFormatEnd
};


class TrackerSettings;
extern TrackerSettings gTrackerSettings;

class TrackerSettings : public Settings {

public:
								TrackerSettings();

		void					SaveSettings(bool onlyIfNonDefault = true);
		const char				*SettingsDirectory();

		bool					ShowDisksIcon();
		void					SetShowDisksIcon(bool);
		bool					DesktopFilePanelRoot();
		void					SetDesktopFilePanelRoot(bool);
		bool					MountVolumesOntoDesktop();
		void					SetMountVolumesOntoDesktop(bool);
		bool					MountSharedVolumesOntoDesktop();
		void					SetMountSharedVolumesOntoDesktop(bool);
		bool					IntegrateNonBootBeOSDesktops();
		void					SetIntegrateNonBootBeOSDesktops(bool);
		bool					IntegrateAllNonBootDesktops();
		void					SetIntegrateAllNonBootDesktops(bool);
		bool					EjectWhenUnmounting();
		void					SetEjectWhenUnmounting(bool);

		bool					ShowVolumeSpaceBar();
		void					SetShowVolumeSpaceBar(bool);
	 	rgb_color				UsedSpaceColor();
		void					SetUsedSpaceColor(rgb_color color);
	 	rgb_color				FreeSpaceColor();
		void					SetFreeSpaceColor(rgb_color color);
	 	rgb_color				WarningSpaceColor();
		void					SetWarningSpaceColor(rgb_color color);

		bool					ShowFullPathInTitleBar();
		void					SetShowFullPathInTitleBar(bool);
		bool					SortFolderNamesFirst();
		void					SetSortFolderNamesFirst(bool);
		bool					ShowSelectionWhenInactive();
		void					SetShowSelectionWhenInactive(bool);
		bool					TransparentSelection();
		void					SetTransparentSelection(bool);
	 	rgb_color				TransparentSelectionColor();
		void					SetTransparentSelectionColor(rgb_color color);

		bool					SingleWindowBrowse();
		void					SetSingleWindowBrowse(bool);
		bool					ShowNavigator();
		void					SetShowNavigator(bool);

		void					RecentCounts(int32 *applications, int32 *documents, int32 *folders);
		void					SetRecentApplicationsCount(int32);
		void					SetRecentDocumentsCount(int32);
		void					SetRecentFoldersCount(int32);

		FormatSeparator			TimeFormatSeparator();
		void					SetTimeFormatSeparator(FormatSeparator);
		DateOrder				DateOrderFormat();
		void					SetDateOrderFormat(DateOrder);
		bool					ClockIs24Hr();
		void					SetClockTo24Hr(bool);

		bool					DontMoveFilesToTrash();
		void					SetDontMoveFilesToTrash(bool);
		bool					AskBeforeDeleteFile();
		void					SetAskBeforeDeleteFile(bool);

		bool					ShowHomeButton();
		void					SetShowHomeButton(bool);
		const char				*HomeButtonDirectory();
		void					SetHomeButtonDirectory(char *);

		bool					DynamicFiltering();
		void					SetDynamicFiltering(bool);
		int32					DynamicFilteringExpressionType();
		void					SetDynamicFilteringExpressionType(int32);
		bool					DynamicFilteringInvert();
		void					SetDynamicFilteringInvert(bool);
		bool					DynamicFilteringIgnoreCase();
		void					SetDynamicFilteringIgnoreCase(bool);

		bool					StaticFiltering();
		void					SetStaticFiltering(bool);

		bool					UndoEnabled();
		void					SetUndoEnabled(bool);
		int32					UndoDepth();
		void					SetUndoDepth(int32);

		bool					WarnInWellKnownDirectories();
		void					SetWarnInWellKnownDirectories(bool);

		bool					IconThemeEnabled();
		void					SetIconThemeEnabled(bool);
		const char				*CurrentIconTheme();
		void					SetCurrentIconTheme(char *theme);
		const char				*IconThemeLookupSequence();
		void					SetIconThemeLookupSequence(char *sequence);

		const char				*LanguageTheme();
		void					SetLanguageTheme(char *theme);

private:
								TrackerSettings(const TrackerSettings &);

		void					LoadSettingsIfNeeded();

		BooleanValueSetting		*fShowDisksIcon;
		BooleanValueSetting		*fMountVolumesOntoDesktop;
		BooleanValueSetting		*fIntegrateNonBootBeOSDesktops;
		BooleanValueSetting		*fIntegrateAllNonBootDesktops;
		BooleanValueSetting		*fDesktopFilePanelRoot;
		BooleanValueSetting		*fMountSharedVolumesOntoDesktop;
		BooleanValueSetting		*fEjectWhenUnmounting;
		BooleanValueSetting		*fShowFullPathInTitleBar;
		BooleanValueSetting		*fShowSelectionWhenInactive;
		BooleanValueSetting		*fTransparentSelection;
		BooleanValueSetting		*fSortFolderNamesFirst;
		BooleanValueSetting		*fSingleWindowBrowse;
		BooleanValueSetting		*fShowNavigator;
		BooleanValueSetting		*f24HrClock;

		ScalarValueSetting		*fRecentApplicationsCount;
		ScalarValueSetting		*fRecentDocumentsCount;
		ScalarValueSetting		*fRecentFoldersCount;
		ScalarValueSetting		*fTimeFormatSeparator;
		ScalarValueSetting		*fDateOrderFormat;

		BooleanValueSetting		*fShowVolumeSpaceBar;
		HexScalarValueSetting	*fUsedSpaceColor;
		HexScalarValueSetting	*fFreeSpaceColor;
		HexScalarValueSetting	*fWarningSpaceColor;

		BooleanValueSetting		*fDontMoveFilesToTrash;
		BooleanValueSetting		*fAskBeforeDeleteFile;

		HexScalarValueSetting	*fTransparentSelectionColor;

		BooleanValueSetting		*fShowHomeButton;
		StringValueSetting		*fHomeButtonDirectory;

		BooleanValueSetting		*fDynamicFiltering;
		ScalarValueSetting		*fDynamicFilteringExpressionType;
		BooleanValueSetting		*fDynamicFilteringInvert;
		BooleanValueSetting		*fDynamicFilteringIgnoreCase;

		BooleanValueSetting		*fStaticFiltering;

		BooleanValueSetting		*fUndoEnabled;
		ScalarValueSetting		*fUndoDepth;

		BooleanValueSetting		*fWarnInWellKnownDirectories;

		BooleanValueSetting		*fIconThemeEnabled;
		StringValueSetting		*fCurrentIconTheme;
		StringValueSetting		*fIconThemeLookupSequence;

		StringValueSetting		*fLanguageTheme;

		Benaphore fInitLock;
		bool fInited;
		bool fSettingsLoaded;
};

} // namespace BPrivate

#endif	/* _TRACKER_SETTINGS_H */
