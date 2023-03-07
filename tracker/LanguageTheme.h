#ifndef _LANGUAGE_THEME_H_
#define _LANGUAGE_THEME_H_

#include "ObjectList.h"
#include <String.h>
#include <Path.h>

#define ENABLE_LANGUAGE_THEMES 1
#if ENABLE_LANGUAGE_THEMES
#define LOCALE(x)			(gLanguageTheme ? gLanguageTheme->Translate(x) : x)
#else
#define LOCALE(x)			x
#endif

class LanguageTheme;
extern LanguageTheme *gLanguageTheme;

typedef struct _language_item {
			_language_item(const char *_native, const char *_locale)
				:	native(_native),
					locale(_locale)
			{
			}

	BString		native;
	BString		locale;
} language_item;

typedef BObjectList<language_item> TranslationList;

class LanguageTheme {
public:
					LanguageTheme(const char *language = NULL);
					~LanguageTheme();

	status_t		SetTo(const char *language);

	status_t		LoadTranslationFile(const char *path);
	status_t		LoadTranslationAttributes(const char *path);
	status_t		LoadTranslationResources(const char *path);
	status_t		ParseTranslations(const char *buffer);

	void			DeescapeString(BString &string);
	const char		*StripComments(const char *commented);
	const char		*FindFirstUnescaped(const char *start, const char *base, const char *search);

	status_t		AddItem(const char *native, const char *locale);
	status_t		RemoveItem(const char *native);
	const char		*Translate(const char *native);

	BPath			GetThemesPath();
	BPath			GetCurrentThemePath();

private:
	BPath			fThemesPath;
	BPath			fCurrentThemePath;
	BString			fCurrentLanguage;
	TranslationList	*fTranslations;
};

#endif
