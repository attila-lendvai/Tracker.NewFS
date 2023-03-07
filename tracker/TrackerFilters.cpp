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

#include "TrackerFilters.h"
#include <Beep.h>
#include <Alert.h>

class TFilters : public Settings {
	public:
		static TFilters *Get();
		void Release();
	
		void LoadFiltersIfNeeded();
		void SaveFilters();

		int32 CountFilters();
		TrackerStringExpressionType ExpressionTypeAt(int32 position);
		const char* ExpressionAt(int32 position);
		bool InvertAt(int32 position);
		bool IgnoreCaseAt(int32 position);
		bool FilterName(const char *name);
		bool FilterEntry(const BEntry *entry);
		bool FilterModel(const Model *model);
		bool FilterPose(const BPose *pose);
		
		void AddFilter(TrackerStringExpressionType expressionType, char* expression, bool invert, bool ignoreCase);
		bool RemoveFilterAt(int32 position);

		TFilters();
		~TFilters();
	
	private:
		static void InitIfNeeded();

		bool fFiltersLoaded;
		ScalarValueSetting *fFilterCount;
		int32 fCount;
		BObjectList<ScalarValueSetting> *fTypeList;
		BObjectList<StringValueSetting> *fExpressionList;
		BObjectList<BooleanValueSetting> *fInvertList;
		BObjectList<BooleanValueSetting> *fIgnoreCaseList;
	
		typedef Settings _inherited;
};

static TFilters gTrackerFilters;

TFilters::TFilters()
	:	Settings("TrackerFilters", "Tracker"),
		fFiltersLoaded(false),
		fCount(0),
		fTypeList(new BObjectList<ScalarValueSetting>(10, true)),
		fExpressionList(new BObjectList<StringValueSetting>(10, true)),
		fInvertList(new BObjectList<BooleanValueSetting>(10, true)),
		fIgnoreCaseList(new BObjectList<BooleanValueSetting>(10, true))
{
}

TFilters::~TFilters()
{
	SaveFilters();
}

void 
TFilters::SaveFilters()
{
	if (fFiltersLoaded)
		_inherited::SaveSettings(false);
}

void 
TFilters::LoadFiltersIfNeeded()
{
	if (fFiltersLoaded)
		return;
	
	Add(fFilterCount = new ScalarValueSetting("FilterCount", 0, "", ""));
	// read out the count first
	TryReadingSettings();

	char name[30];
	char* nameString = NULL;
	for (int32 i = 0; i < fFilterCount->Value(); i++) {
		sprintf(name, "%ld-ExpressionType", i);
		nameString = new char[strlen(name) + 1];
		strcpy(nameString, name);
		fTypeList->AddItem(new ScalarValueSetting(nameString, kStartsWith, "", ""));
		Add(fTypeList->ItemAt(fCount));
		sprintf(name, "%ld-ExpressionString", i);
		nameString = new char[strlen(name) + 1];
		strcpy(nameString, name);
		fExpressionList->AddItem(new StringValueSetting(nameString, ".", "", ""));
		Add(fExpressionList->ItemAt(fCount));
		sprintf(name, "%ld-Invert", i);
		nameString = new char[strlen(name) + 1];
		strcpy(nameString, name);
		fInvertList->AddItem(new BooleanValueSetting(nameString, false));
		Add(fInvertList->ItemAt(fCount));
		sprintf(name, "%ld-IgnoreCase", i);
		nameString = new char[strlen(name) + 1];
		strcpy(nameString, name);
		fIgnoreCaseList->AddItem(new BooleanValueSetting(nameString, true));
		Add(fIgnoreCaseList->ItemAt(fCount));
		fCount++;
	}

	// and read all filters now
	TryReadingSettings();

	fFiltersLoaded = true;
}

int32
TFilters::CountFilters()
{
	return fCount;
}

TrackerStringExpressionType
TFilters::ExpressionTypeAt(int32 position)
{
	ScalarValueSetting *value = fTypeList->ItemAt(position);
	if (!value)
		return kNone;
	TrackerStringExpressionType typeArray[] = {	kStartsWith, kEndsWith, kContains, kGlobMatch, kRegexpMatch};
	return typeArray[value->Value()];	
}

const char*
TFilters::ExpressionAt(int32 position)
{
	StringValueSetting *value = fExpressionList->ItemAt(position);
	if (!value)
		return NULL;
	return value->Value();
}

bool
TFilters::InvertAt(int32 position)
{
	BooleanValueSetting *value = fInvertList->ItemAt(position);
	if (!value)
		return false;
	return value->Value();
}

bool
TFilters::IgnoreCaseAt(int32 position)
{
	BooleanValueSetting *value = fIgnoreCaseList->ItemAt(position);
	if (!value)
		return true;
	return value->Value();
}

bool
TFilters::FilterName(const char *name)
{
	if (!fCount)
		return true;
	
	TrackerString match(name);
	
	for (int32 i = 0; i < fCount; i++) {
		TrackerStringExpressionType expressionType = ExpressionTypeAt(i);
		if (expressionType == kNone)
			continue;

		const char* expression = ExpressionAt(i);
		if (expression == NULL)
			continue;
		
		if (match.Matches(expression, !IgnoreCaseAt(i), expressionType) ^ InvertAt(i))
			return false;
	}
	return true;
}

bool
TFilters::FilterEntry(const BEntry *entry)
{
	char buffer[B_FILE_NAME_LENGTH];
	entry->GetName(buffer);
	return FilterName(buffer);
}

bool
TFilters::FilterModel(const Model *model)
{
	return FilterName(model->Name());
}

bool
TFilters::FilterPose(const BPose *pose)
{
	return FilterName(pose->TargetModel()->Name());
}

void
TFilters::AddFilter(TrackerStringExpressionType expressionType, char* expression, bool invert, bool ignoreCase)
{
	char name[30];
	sprintf(name, "%ld-ExpressionType", fCount);
	char* nameString = new char[strlen(name) + 1];
	strcpy(nameString, name);
	ScalarValueSetting *scalarsetting = new ScalarValueSetting(nameString, kStartsWith, "", "");
	scalarsetting->ValueChanged(expressionType);
	fTypeList->AddItem(scalarsetting);
	Add(scalarsetting);

	sprintf(name, "%ld-ExpressionString", fCount);
	nameString = new char[strlen(name) + 1];
	strcpy(nameString, name);
	StringValueSetting *stringsetting = new StringValueSetting(nameString, ".", "", "");
	stringsetting->ValueChanged(expression);
	fExpressionList->AddItem(stringsetting);
	Add(stringsetting);

	sprintf(name, "%ld-Invert", fCount);
	nameString = new char[strlen(name) + 1];
	strcpy(nameString, name);
	BooleanValueSetting *boolsetting = new BooleanValueSetting(nameString, false);
	boolsetting->SetValue(invert);
	fInvertList->AddItem(boolsetting);
	Add(boolsetting);

	sprintf(name, "%ld-IgnoreCase", fCount);
	nameString = new char[strlen(name) + 1];
	strcpy(nameString, name);
	boolsetting = new BooleanValueSetting(nameString, true);
	boolsetting->SetValue(ignoreCase);
	fIgnoreCaseList->AddItem(boolsetting);
	Add(boolsetting);

	fCount++;
	fFilterCount->ValueChanged(fCount);
}

bool
TFilters::RemoveFilterAt(int32 position)
{
	fTypeList->RemoveItemAt(position);
	fExpressionList->RemoveItemAt(position);
	fInvertList->RemoveItemAt(position);
	fIgnoreCaseList->RemoveItemAt(position);

	bool result = true;
	char name[30];
	sprintf(name, "%ld-ExpressionType", position);
	char* nameString = new char[strlen(name) + 1];
	strcpy(nameString, name);
	result = result && Remove(nameString);
	
	sprintf(name, "%ld-ExpressionString", position);
	nameString = new char[strlen(name) + 1];
	strcpy(nameString, name);
	result = result && Remove(nameString);
	
	sprintf(name, "%ld-Invert", position);
	nameString = new char[strlen(name) + 1];
	strcpy(nameString, name);
	result = result && Remove(nameString);
	
	sprintf(name, "%ld-IgnoreCase", position);
	nameString = new char[strlen(name) + 1];
	strcpy(nameString, name);
	result = result && Remove(nameString);

	fCount--;
	fFilterCount->ValueChanged(fCount);
	return result;
}

TrackerFilters::TrackerFilters() { gTrackerFilters.LoadFiltersIfNeeded(); }
void TrackerFilters::SaveFilters() { gTrackerFilters.SaveFilters(); }
int32 TrackerFilters::CountFilters() { return gTrackerFilters.CountFilters(); }
TrackerStringExpressionType TrackerFilters::ExpressionTypeAt(int32 position) { return gTrackerFilters.ExpressionTypeAt(position); }
const char* TrackerFilters::ExpressionAt(int32 position) { return gTrackerFilters.ExpressionAt(position); }
bool TrackerFilters::InvertAt(int32 position) { return gTrackerFilters.InvertAt(position); }
bool TrackerFilters::IgnoreCaseAt(int32 position) { return gTrackerFilters.IgnoreCaseAt(position); }
bool TrackerFilters::FilterName(const char *name) { return gTrackerFilters.FilterName(name); }
bool TrackerFilters::FilterEntry(const BEntry *entry) { return gTrackerFilters.FilterEntry(entry); }
bool TrackerFilters::FilterPose(const BPose *pose) { return gTrackerFilters.FilterPose(pose); }
bool TrackerFilters::FilterModel(const Model *model) { return gTrackerFilters.FilterModel(model); }
void TrackerFilters::AddFilter(TrackerStringExpressionType expressionType, char* expression, bool invert, bool ignoreCase) { gTrackerFilters.AddFilter(expressionType, expression, invert, ignoreCase); }
bool TrackerFilters::RemoveFilterAt(int32 position) { return gTrackerFilters.RemoveFilterAt(position); };
