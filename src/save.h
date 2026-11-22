/** @file src/save.h Save definitions. */

#ifndef SAVE_H
#define SAVE_H

#ifdef __3DS__
extern bool SaveGame_WriteStream(FILE *fp, char *description);
extern void SaveGame_RecoverFile(const char *filename);
#endif
extern bool SaveGame_SaveFile(char *filename, char *description);

#endif /* SAVE_H */
