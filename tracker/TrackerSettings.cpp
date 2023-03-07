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


#include "TrackerSettings.h"
#include "WidgetAttributeText.h"

namespace BPrivate {

static TrackerSettings gTrackerSettings;

rgb_color ValueToColor(int32 value)
{
	rgb_color color;
	color.alpha = static_cast<uchar>((value >> 24L) & 0xff);
	color.red = static_cast<uchar>((value >> 16L) & 0xff);
	color.green = static_cast<uchar>((value >> 8L) & 0xff);
	color.blue = static_cast<uchar>(value & 0xff);

	// zero alpha is invalid
	if (color.alpha == 0)
		color.alpha = 192;

	return color;
}

int32 ColorToValue(rgb_color color)
{
	// zero alpha is invalid
	if (color.alpha == 0)
		color.alpha = 192;

	return	color.alpha << 24L
			| color.red << 16L
			| color.green << 8L
			| color.blue;
}


TrackerSettings::TrackerSettings()
	:	Settings("TrackerSettings", "Tracker"),
		fInited(false),
		fSettingsLoaded(false)
{
	LoadSettingsIfNeeded();
}


TrackerSettings::TrackerSettings(const TrackerSettings &)
	:	Settings("", "")
{
	// Placeholder copy constructor to prevent others from accidentally using the
	// default copy constructor.  Note, the DEBUGGER call is for the off chance that
	// a TTrackerState method (or friend) tries to make a copy.
	DEBUGGER("Don't make a copy of this!");
}


void 
TrackerSettings::LoadSettingsIfNeeded()
{
	if (fSettingsLoaded)
		return;

	Add(fShowDisksIcon = new BooleanValueSetting("ShowDisksIcon", false));
	Add(fMountVolumesOntoDesktop = new BooleanValueSetting("MountVolumesOntoDesktop", true));
	Add(fMountSharedVolumesOntoDesktop = new BooleanValueSetting("MountSharedVolumesOntoDesktop", false));
	Add(fIntegrateNonBootBeOSDesktops = new BooleanValueSetting("IntegrateNonBootBeOSDesktops", true));
	Add(fIntegrateAllNonBootDesktops = new BooleanValueSetting("IntegrateAllNonBootDesktops", false));
	Add(fEjectWhenUnmounting = new BooleanValueSetting("EjectWhenUnmounting", true));
	Add(fDesktopFilePanelRoot = new BooleanValueSetting("DesktopFilePanelRoot", true));
	Add(fShowFullPathInTitleBar = new BooleanValueSetting("ShowFullPathInTitleBar", false));
	Add(fShowSelectionWhenInactive = new BooleanValueSetting("ShowSelectionWhenInactive", true));
	Add(fTransparentSelection = new BooleanValueSetting("TransparentSelection", false));
	Add(fSortFolderNamesFirst = new BooleanValueSetting("SortFolderNamesFirst", true));
 	Add(fSingleWindowBrowse = new BooleanValueSetting("SingleWindowBrowse", true));
	Add(fShowNavigator = new BooleanValueSetting("ShowNavigator", true));
	
	Add(fRecentApplicationsCount = new ScalarValueSetting("RecentApplications", 10, "", ""));
	Add(fRecentDocumentsCount = new ScalarValueSetting("RecentDocuments", 10, "", ""));
	Add(fRecentFoldersCount = new ScalarValueSetting("RecentFolders", 10, "", ""));

	Add(fTimeFormatSeparator = new ScalarValueSetting("TimeFormatSeparator", 3, "", ""));
	Add(fDateOrderFormat = new ScalarValueSetting("DateOrderFormat", 2, "", ""));
	Add(f24HrClock = new BooleanValueSetting("24HrClock", false));

	Add(fShowVolumeSpaceBar = new BooleanValueSetting("ShowVolumeSpaceBar", false));

	Add(fUsedSpaceColor = new HexScalarValueSetting("UsedSpaceColor", 0xc000cb00, "", ""));
	Add(fFreeSpaceColor = new HexScalarValueSetting("FreeSpaceColor", 0xc0ffffff, "", ""));
	Add(fWarningSpaceColor = new HexScalarValueSetting("WarningSpaceColor", 0xc0cb0000, "", ""));

	Add(fDontMoveFilesToTrash = new BooleanValueSetting("DontMoveFilesToTrash", false));
	Add(fAskBeforeDeleteFile = new BooleanValueSetting("AskBeforeDeleteFile", true));

	Add(fTransparentSelectionColor = new HexScalarValueSetting("TransparentSelectionColor", 0x5050505f, "", ""));
	
	Add(fShowHomeButton = new BooleanValueSetting("ShowHomeButton", false));
	Add(fHomeButtonDirectory = new StringValueSetting("HomeButtonDirectory", "/boot/home", "", ""));

	Add(fDynamicFiltering = new BooleanValueSetting("DynamicFiltering", true));
	Add(fDynamicFilteringExpressionType = new ScalarValueSetting("DynamicFilteringExpressionType", 3, "", ""));
	Add(fDynamicFilteringInvert = new BooleanValueSetting("DynamicFilteringInvert", false));
	Add(fDynamicFilteringIgnoreCase = new BooleanValueSetting("DynamicFilteringIgnoreCase", true));

	Add(fStaticFiltering = new BooleanValueSetting("StaticFiltering", false));
	
	Add(fUndoEnabled = new BooleanValueSetting("UndoEnabled", true));
	Add(fUndoDepth = new ScalarValueSetting("UndoDepth", 10, "", "", 1));
	
	Add(fWarnInWellKnownDirectories = new BooleanValueSetting("WarnInWellKnownDirectories", true));
	
	Add(fIconThemeEnabled = new BooleanValueSetting("IconThemeEnabled", false));
	Add(fCurrentIconTheme = new StringValueSetting("CurrentIconTheme", "", "", ""));
	Add(fIconThemeLookupSequence = new StringValueSetting("IconThemeLookupSequence", "badecf", "", ""));
	
	Add(fLanguageTheme = new StringValueSetting("LanguageTheme", "English", "", ""));
	
	TryReadingSettings();
	
	NameAttributeText::SetSortFolderNamesFirst(fSortFolderNamesFirst->Value());
	
	fSettingsLoaded = true;
}


void
TrackerSettings::SaveSettings(bool onlyIfNonDefault)
{
	if (fSettingsLoaded)
		Settings::SaveSettings(onlyIfNonDefault);
}


const char *
TrackerSettings::SettingsDirectory()
{
	return Settings::SettingsDirectory();
}


bool
TrackerSettings::ShowDisksIcon()
{
	return fShowDisksIcon->Value();
}


void
TrackerSettings::SetShowDisksIcon(bool enabled)
{
	fShowDisksIcon->SetValue(enabled);
}


bool
TrackerSettings::DesktopFilePanelRoot()
{
	return fDesktopFilePanelRoot->Value();
}


void 
TrackerSettings::SetDesktopFilePanelRoot(bool enabled)
{
	fDesktopFilePanelRoot->SetValue(enabled);
}


bool
TrackerSettings::MountVolumesOntoDesktop()
{
	return fMountVolumesOntoDesktop->Value();
}


void 
TrackerSettings::SetMountVolumesOntoDesktop(bool enabled)
{
	fMountVolumesOntoDesktop->SetValue(enabled);
}


bool
TrackerSettings::MountSharedVolumesOntoDesktop()
{
	return fMountSharedVolumesOntoDesktop->Value();
}


void 
TrackerSettings::SetMountSharedVolumesOntoDesktop(bool enabled)
{
	fMountSharedVolumesOntoDesktop->SetValue(enabled);
}


bool
TrackerSettings::IntegrateNonBootBeOSDesktops()
{
	return fIntegrateNonBootBeOSDesktops->Value();
}


void 
TrackerSettings::SetIntegrateNonBootBeOSDesktops(bool enabled)
{
	fIntegrateNonBootBeOSDesktops->SetValue(enabled);
}


bool
TrackerSettings::EjectWhenUnmounting()
{
	return fEjectWhenUnmounting->Value();
}


void
TrackerSettings::SetEjectWhenUnmounting(bool enabled)
{
	fEjectWhenUnmounting->SetValue(enabled);
}


bool
TrackerSettings::IntegrateAllNonBootDesktops()
{
	return fIntegrateAllNonBootDesktops->Value();
}


bool
TrackerSettings::ShowVolumeSpaceBar()
{
	return fShowVolumeSpaceBar->Value();
}


void
TrackerSettings::SetShowVolumeSpaceBar(bool enabled)
{
	fShowVolumeSpaceBar->SetValue(enabled);
}


rgb_color
TrackerSettings::UsedSpaceColor()
{
	return ValueToColor(fUsedSpaceColor->Value());
}


void
TrackerSettings::SetUsedSpaceColor(rgb_color color)
{
	if (color.alpha == 0)
		color.alpha = 192;
	
	fUsedSpaceColor->ValueChanged(ColorToValue(color));
}


rgb_color
TrackerSettings::FreeSpaceColor()
{
	return ValueToColor(fFreeSpaceColor->Value());
}


void
TrackerSettings::SetFreeSpaceColor(rgb_color color)
{
	if (color.alpha == 0)
		color.alpha = 192;
	
	fFreeSpaceColor->ValueChanged(ColorToValue(color));
}


rgb_color
TrackerSettings::WarningSpaceColor()
{
	return ValueToColor(fWarningSpaceColor->Value());
}


void
TrackerSettings::SetWarningSpaceColor(rgb_color color)
{
	if (color.alpha == 0)
		color.alpha = 192;
	
	fWarningSpaceColor->ValueChanged(ColorToValue(color));
}


bool
TrackerSettings::ShowFullPathInTitleBar()
{
	return fShowFullPathInTitleBar->Value();
}


void
TrackerSettings::SetShowFullPathInTitleBar(bool enabled)
{
	fShowFullPathInTitleBar->SetValue(enabled);
}


bool
TrackerSettings::SortFolderNamesFirst()
{
	return fSortFolderNamesFirst->Value();
}


void
TrackerSettings::SetSortFolderNamesFirst(bool enabled)
{
	fSortFolderNamesFirst->SetValue(enabled);
	NameAttributeText::SetSortFolderNamesFirst(enabled);
}


bool
TrackerSettings::ShowSelectionWhenInactive()
{
	return fShowSelectionWhenInactive->Value();
}


void
TrackerSettings::SetShowSelectionWhenInactive(bool enabled)
{
	fShowSelectionWhenInactive->SetValue(enabled);
}


bool
TrackerSettings::TransparentSelection()
{
	return fTransparentSelection->Value();
}


void
TrackerSettings::SetTransparentSelection(bool enabled)
{
	fTransparentSelection->SetValue(enabled);
}


rgb_color
TrackerSettings::TransparentSelectionColor()
{
	return ValueToColor(fTransparentSelectionColor->Value());
}


void
TrackerSettings::SetTransparentSelectionColor(rgb_color color)
{
	if (color.alpha == 0)
		color.alpha = 90;
	
	fTransparentSelectionColor->ValueChanged(ColorToValue(color));
}


bool
TrackerSettings::SingleWindowBrowse()
{
	return fSingleWindowBrowse->Value();
}


void
TrackerSettings::SetSingleWindowBrowse(bool enabled)
{
	fSingleWindowBrowse->SetValue(enabled);
}


bool
TrackerSettings::ShowNavigator()
{
	return fShowNavigator->Value();
}


void
TrackerSettings::SetShowNavigator(bool enabled)
{
	fShowNavigator->SetValue(enabled);
}


void
TrackerSettings::RecentCounts(int32 *applications, int32 *documents, int32 *folders)
{
	if (applications)
		*applications = fRecentApplicationsCount->Value();
	if (documents)
		*documents = fRecentDocumentsCount->Value();
	if (folders)
		*folders = fRecentFoldersCount->Value();
}


void  
TrackerSettings::SetRecentApplicationsCount(int32 count)
{
	fRecentApplicationsCount->ValueChanged(count);
}


void  
TrackerSettings::SetRecentDocumentsCount(int32 count)
{
	fRecentDocumentsCount->ValueChanged(count);
}


void  
TrackerSettings::SetRecentFoldersCount(int32 count)
{
	fRecentFoldersCount->ValueChanged(count);
}


FormatSeparator
TrackerSettings::TimeFormatSeparator()
{
	return (FormatSeparator)fTimeFormatSeparator->Value();
}


void
TrackerSettings::SetTimeFormatSeparator(FormatSeparator separator)
{
	fTimeFormatSeparator->ValueChanged((int32)separator);
}


DateOrder
TrackerSettings::DateOrderFormat()
{
	return (DateOrder)fDateOrderFormat->Value();
}


void
TrackerSettings::SetDateOrderFormat(DateOrder order)
{
	fDateOrderFormat->ValueChanged((int32)order);
}


bool
TrackerSettings::ClockIs24Hr()
{
	return f24HrClock->Value();
}


void
TrackerSettings::SetClockTo24Hr(bool enabled)
{
	f24HrClock->SetValue(enabled);
}


bool
TrackerSettings::DontMoveFilesToTrash()
{
	return fDontMoveFilesToTrash->Value();
}


void
TrackerSettings::SetDontMoveFilesToTrash(bool enabled)
{
	fDontMoveFilesToTrash->SetValue(enabled);
}


bool
TrackerSettings::AskBeforeDeleteFile()
{
	return fAskBeforeDeleteFile->Value();
}


void
TrackerSettings::SetAskBeforeDeleteFile(bool enabled)
{
	fAskBeforeDeleteFile->SetValue(enabled);
}

bool
TrackerSettings::ShowHomeButton()
{
	return fShowHomeButton->Value();
}

void
TrackerSettings::SetShowHomeButton(bool enabled)
{
	fShowHomeButton->SetValue(enabled);
}

const char *
TrackerSettings::HomeButtonDirectory()
{
	return fHomeButtonDirectory->Value();
}

void
TrackerSettings::SetHomeButtonDirectory(char* in_string)
{
	fHomeButtonDirectory->ValueChanged(in_string);
}

bool
TrackerSettings::DynamicFiltering()
{
	return fDynamicFiltering->Value();
}

void
TrackerSettings::SetDynamicFiltering(bool enabled)
{
	fDynamicFiltering->SetValue(enabled);
}

int32
TrackerSettings::DynamicFilteringExpressionType()
{
	return fDynamicFilteringExpressionType->Value();
}

void
TrackerSettings::SetDynamicFilteringExpressionType(int32 expressiontype)
{
	fDynamicFilteringExpressionType->ValueChanged(expressiontype);
}

bool
TrackerSettings::DynamicFilteringInvert()
{
	return fDynamicFilteringInvert->Value();
}

void
TrackerSettings::SetDynamicFilteringInvert(bool enabled)
{
	fDynamicFilteringInvert->SetValue(enabled);
}

bool
TrackerSettings::DynamicFilteringIgnoreCase()
{
	return fDynamicFilteringIgnoreCase;
}

void
TrackerSettings::SetDynamicFilteringIgnoreCase(bool enabled)
{
	fDynamicFilteringIgnoreCase->SetValue(enabled);
}

bool
TrackerSettings::StaticFiltering()
{
	return fStaticFiltering->Value();
}

void
TrackerSettings::SetStaticFiltering(bool enabled)
{
	fStaticFiltering->SetValue(enabled);
}

bool
TrackerSettings::UndoEnabled()
{
	return fUndoEnabled->Value();
}

void
TrackerSettings::SetUndoEnabled(bool enabled)
{
	fUndoEnabled->SetValue(enabled);
}

int32
TrackerSettings::UndoDepth()
{
	return fUndoDepth->Value();
}

void
TrackerSettings::SetUndoDepth(int32 depth)
{
	fUndoDepth->ValueChanged(depth);
}

bool
TrackerSettings::WarnInWellKnownDirectories()
{
	return fWarnInWellKnownDirectories->Value();
}

void
TrackerSettings::SetWarnInWellKnownDirectories(bool enabled)
{
	fWarnInWellKnownDirectories->SetValue(enabled);
}

bool
TrackerSettings::IconThemeEnabled()
{
	return fIconThemeEnabled->Value();
}

void
TrackerSettings::SetIconThemeEnabled(bool enabled)
{
	fIconThemeEnabled->SetValue(enabled);
}

const char *
TrackerSettings::CurrentIconTheme()
{
	return fCurrentIconTheme->Value();
}

void
TrackerSettings::SetCurrentIconTheme(char *theme)
{
	fCurrentIconTheme->ValueChanged(theme);
}

const char *
TrackerSettings::IconThemeLookupSequence()
{
	return fIconThemeLookupSequence->Value();
}

void
TrackerSettings::SetIconThemeLookupSequence(char *sequence)
{
	fIconThemeLookupSequence->ValueChanged(sequence);
}

const char *
TrackerSettings::LanguageTheme()
{
	return fLanguageTheme->Value();
}

void
TrackerSettings::SetLanguageTheme(char *new_locale)
{
	fLanguageTheme->ValueChanged(new_locale);
}

} // namespace BPrivate
