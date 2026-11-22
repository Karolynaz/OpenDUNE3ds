/** @file src/table/strings_lt.h Lithuanian localization tables. */

#ifndef TABLE_STRINGS_LT_H
#define TABLE_STRINGS_LT_H

#include "types.h"

/** Number of entries covered by #String_LoadLithuanian (0..524: DUNE, MESSAGE, INTRO, TEXTH, TEXTA, TEXTO). */
#define STRINGS_LT_COUNT 525

extern void String_LoadLithuanian(uint16 *offsets, char *buffer, uint32 bufferSize, uint16 *countOut);
extern const char *Strings_LT_TranslateMentatTopic(const char *engTitle);
extern const char *Strings_LT_LookupMentatDesc(const char *wsaFilename);
extern const char *Strings_LT_TranslateRegionText(const char *engText);

#endif /* TABLE_STRINGS_LT_H */
