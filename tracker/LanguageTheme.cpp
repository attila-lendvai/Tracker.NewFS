#include <File.h>
#include <FindDirectory.h>
#include <fs_attr.h>
#include <InterfaceDefs.h>
#include <Node.h>
#include <Resources.h>
#include <stdio.h>
#include <String.h>
#include "LanguageTheme.h"
#include "TrackerSettings.h"

LanguageTheme *gLanguageTheme = NULL;

static int
TranslationNativeCompare(const language_item *first, const language_item *second)
{
	return first->native.Compare(second->native);
}

static int
TranslationLocaleCompare(const language_item *first, const language_item *second)
{
	return first->locale.Compare(second->locale);
}

LanguageTheme::LanguageTheme(const char *language)
{
	fTranslations = new TranslationList(20, true);
	SetTo(language);
}

LanguageTheme::~LanguageTheme()
{
	delete fTranslations;
}

status_t
LanguageTheme::SetTo(const char *language)
{
	fTranslations->MakeEmpty();
	if (!language)
		fCurrentLanguage = gTrackerSettings.LanguageTheme();
	else
		fCurrentLanguage = language;
	
	find_directory(B_USER_SETTINGS_DIRECTORY, &fThemesPath, true);
	fThemesPath.Append(gTrackerSettings.SettingsDirectory());
	fThemesPath.Append("LanguageThemes");
	
	fCurrentThemePath = fThemesPath;
	fCurrentThemePath.Append(fCurrentLanguage.String());
	
	// try if this is a resource file, else read as raw
	if (LoadTranslationResources(fCurrentThemePath.Path()) != B_OK)
		if (LoadTranslationFile(fCurrentThemePath.Path()) != B_OK)
			return B_ERROR;
	
	return B_OK;
}

status_t
LanguageTheme::LoadTranslationFile(const char *path)
{
	if (!path)
		return B_BAD_VALUE;
	
	BFile file(path, B_READ_ONLY);
	if (file.InitCheck() != B_OK || !file.IsReadable())
		return B_ERROR;
	
	off_t size;
	if (file.GetSize(&size) != B_OK || size <= 0)
		return B_ERROR;
	
	char *buffer = new char[size];
	file.Read(buffer, size);
	
	status_t result = ParseTranslations(buffer);
	delete buffer;
	
	return result;
}

status_t
LanguageTheme::LoadTranslationAttributes(const char *path)
{
	if (!path)
		return B_BAD_VALUE;
	
	BNode node(path);
	if (node.InitCheck() != B_OK)
		return B_ERROR;
	
	char buffer[B_ATTR_NAME_LENGTH];
	while (node.GetNextAttrName(buffer) == B_OK) {
		attr_info info;
		if (node.GetAttrInfo(buffer, &info) != B_OK)
			continue;
		
		char locale[info.size];
		if (node.ReadAttr(buffer, info.type, 1, locale, info.size) != info.size)
			continue;
		
		AddItem(buffer, locale);
	}
	
	return B_OK;
}

status_t
LanguageTheme::LoadTranslationResources(const char *path)
{
	if (!path)
		return B_BAD_VALUE;
	
	BFile file(path, B_READ_ONLY);
	if (file.InitCheck() != B_OK)
		return B_ERROR;
	
	BResources resources;
	if (resources.SetTo(&file) != B_OK)
		return B_ERROR;
	
	int index = 0;
	resources.PreloadResourceType(B_STRING_TYPE);
	
	while (true) {
		const char *name = NULL;
		type_code type = B_STRING_TYPE;
		size_t size = 0;
		int32 id = 0;
		
		if (!resources.GetResourceInfo(index++, &type, &id, &name, &size))
			break;
		
		if (type != B_STRING_TYPE)
			continue;
		
		const char *data = NULL;
		data = (const char *)resources.LoadResource(type, id, &size);
		if (!data || size <= 0)
			continue;
		
		AddItem(StripComments(name), StripComments(data));
	}
	
	return B_OK;
}

void
LanguageTheme::DeescapeString(BString &string)
{
	char temp[2] = { 0, 0 };
	char temp2[2] = { 1, 0 };
	
	string.ReplaceAll("\\\\", temp2);
	temp[0] = '\n'; string.ReplaceAll("\\n", temp);
	temp[0] = '\r'; string.ReplaceAll("\\r", temp);
	temp[0] = '\t'; string.ReplaceAll("\\t", temp);
	temp[0] = '\v'; string.ReplaceAll("\\v", temp);
	temp[0] = '\b'; string.ReplaceAll("\\b", temp);
	temp[0] = '\f'; string.ReplaceAll("\\f", temp);
	temp[0] = '\a'; string.ReplaceAll("\\a", temp);
	temp[0] = '\''; string.ReplaceAll("\\'", temp);
	temp[0] = '\"'; string.ReplaceAll("\\\"", temp);
	temp[0] = '\?'; string.ReplaceAll("\\?", temp);
	temp[0] = '\\'; string.ReplaceAll(temp2, temp);
}

BString gStippedString;

const char *
LanguageTheme::StripComments(const char *commented)
{
	gStippedString.SetTo(commented);
	int start = gStippedString.FindFirst(" [");
	if (start < 0)
		start = gStippedString.FindFirst("[");
	if (start < 0)
		return commented;
	
	int end = gStippedString.FindFirst("] ", start);
	if (end < 0)
		end = gStippedString.FindFirst("]", start);
	if (end < 0)
		return commented;
	
	gStippedString.Remove(start, end - start + 1);
	return gStippedString.String();
}

const char *
LanguageTheme::FindFirstUnescaped(const char *start, const char *base, const char *search)
{
	const char *result = start;
	while (true) {
		result = strstr(result, search);
		if (!result)
			return NULL;
		else if (result == base)
			// at the very beginning, don't look at - 1
			break;
		else if (*(result - 1) != '\\')
			// unescaped
			break;
		else {
			// escaped
			const char *offset = result;
			int count = 0;
			while (offset > start) {
				if (*(--offset) == '\\')
					count++;
				else
					break;
			}
			
			// if we have an even count we don't escape the quote
			if (count % 2 == 0)
				break;
		}
		
		// we appearantly found an escaped quote, search for the next one
		result++;
	}
	
	return result;
}

status_t
LanguageTheme::ParseTranslations(const char *buffer)
{
	if (!buffer)
		return B_BAD_VALUE;
	
	BString string = buffer;
	
	// preprocess strings
	string.ReplaceAll("\"B_UTF8_ELLIPSIS", B_UTF8_ELLIPSIS"\"");
	
	// parse strings
	const char *base = string.String();
	const char *start = base;
	const char *end = start - 1;
	
	while (true) {
		start = FindFirstUnescaped(end + 1, base, "\"");
		if (start && start > end) {
			end = FindFirstUnescaped(start + 1, base, "\"");
			
			if (end && end > start) {
				BString native;
				native.SetTo(start + 1, end - start - 1);
				
				start = FindFirstUnescaped(end + 1, base, "\"");
				if (start && start > end) {
					end = FindFirstUnescaped(start + 1, base, "\"");
					if (end && end > start) {
						BString locale;
						locale.SetTo(start + 1, end - start - 1);
						DeescapeString(native);
						DeescapeString(locale);
						AddItem(native.String(), locale.String());
					} else
						return B_ERROR;
				} else
					return B_ERROR;
			} else
				return B_ERROR;
		} else
			// at the end
			break;
	}
	
	return B_OK;
}

status_t
LanguageTheme::AddItem(const char *native, const char *locale)
{
	language_item *item = new language_item(native, locale);
	if (!fTranslations->BinaryInsertUnique(item, &TranslationNativeCompare))
		delete item;
	
	return B_OK;
}

status_t
LanguageTheme::RemoveItem(const char *native)
{
	language_item item(native, NULL);
	const language_item *_found = fTranslations->BinarySearch(item, &TranslationNativeCompare);
	language_item *found = const_cast<language_item *>(_found);
	
	if (found)
		fTranslations->RemoveItem(found);
	
	return B_OK;
}

#define TIME_TRANSLATIONS 0
#if TIME_TRANSLATIONS
bigtime_t gTranslateTime = 0;
#endif

const char *
LanguageTheme::Translate(const char *native)
{
	//printf("native: %s ->", native);
#if TIME_TRANSLATIONS
	bigtime_t time = system_time();
#endif
	language_item item(native, NULL);
	const language_item *_found = fTranslations->BinarySearch(item, &TranslationNativeCompare);
	language_item *found = const_cast<language_item *>(_found);
#if TIME_TRANSLATIONS
	gTranslateTime += system_time() - time;
	printf("time: %lld %lld\n", system_time() - time, gTranslateTime);
#endif
	
	//printf(" %x -> %d -> %s\n", found, (found ? found->locale.Length() : -1), (found ? found->locale.String() : ""));
	if (!found || found->locale.Length() <= 0)
		return native;
	
	return found->locale.String();
}

BPath
LanguageTheme::GetThemesPath()
{
	return fThemesPath;
}

BPath
LanguageTheme::GetCurrentThemePath()
{
	return fCurrentThemePath;
}
