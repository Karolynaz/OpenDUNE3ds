/** @file src/save.c Save routines. */

#include <stdio.h>
#ifdef __3DS__
#include <errno.h>
#include <sys/stat.h>
#endif
#include <string.h>
#include "types.h"

#include "save.h"

#include "house.h"
#include "map.h"
#include "opendune.h"
#include "os/endian.h"
#include "os/error.h"
#include "os/math.h"
#include "os/strings.h"
#include "pool/pool.h"
#include "pool/structure.h"
#include "pool/unit.h"
#include "saveload/saveload.h"
#include "sprites.h"
#include "structure.h"
#include "team.h"
#include "unit.h"
#include "file.h"

/**
 * Save a chunk of data.
 * @param fp The file to save to.
 * @param header The chunk identification string (4 chars, always).
 * @param saveProc The proc to call to generate the content of the chunk.
 * @return True if and only if all bytes were written successful.
 */
static bool Save_Chunk(FILE *fp, const char *header, bool (*saveProc)(FILE *fp))
{
	uint32 position;
	uint32 length;
	uint32 lengthSwapped;

	if (fwrite(header, 4, 1, fp) != 1) return false;

	/* Reserve the length field */
	length = 0;
	if (fwrite(&length, 4, 1, fp) != 1) return false;

	/* Store the content of the chunk, and remember the length */
	long start = ftell(fp);
	if (start < 0 || !saveProc(fp)) return false;
	long end = ftell(fp);
	if (end < start) return false;
	position = start;
	length = end - start;

	/* Ensure we are word aligned */
	if ((length & 1) == 1) {
		uint8 empty = 0;
		if (fwrite(&empty, 1, 1, fp) != 1) return false;
	}

	/* Write back the chunk size */
	if (fseek(fp, position - 4, SEEK_SET) != 0) return false;
	lengthSwapped = HTOBE32(length);
	if (fwrite(&lengthSwapped, 4, 1, fp) != 1) return false;
	return fseek(fp, 0, SEEK_END) == 0;
}

/**
 * Save the game for real. It creates all the required chunks and stores them
 *  to the file. It updates the field lengths where needed.
 *
 * @param fp The file to save to.
 * @param description The description of the savegame.
 * @return True if and only if all bytes were written successful.
 */
static bool Save_Main(FILE *fp, char *description)
{
	uint32 length;
	uint32 lengthSwapped;

	/* Write the 'FORM' chunk (in which all other chunks are) */
	if (fwrite("FORM", 4, 1, fp) != 1) return false;
	/* Write zero length for now. We come back to this value before closing */
	length = 0;
	if (fwrite(&length, 4, 1, fp) != 1) return false;

	/* Write the 'SCEN' chunk. Never contains content. */
	if (fwrite("SCEN", 4, 1, fp) != 1) return false;

	/* Write the 'NAME' chunk. Keep ourself word-aligned. */
	if (fwrite("NAME", 4, 1, fp) != 1) return false;
	length = min(255, (uint32)strlen(description) + 1);
	lengthSwapped = HTOBE32(length);
	if (fwrite(&lengthSwapped, 4, 1, fp) != 1) return false;
	if (fwrite(description, length, 1, fp) != 1) return false;
	/* Ensure we are word aligned */
	if ((length & 1) == 1) {
		uint8 empty = 0;
		if (fwrite(&empty, 1, 1, fp) != 1) return false;
	}

	/* Store all additional chunks */
	if (!Save_Chunk(fp, "INFO", &Info_Save)) return false;
	if (!Save_Chunk(fp, "PLYR", &House_Save)) return false;
	if (!Save_Chunk(fp, "UNIT", &Unit_Save)) return false;
	if (!Save_Chunk(fp, "BLDG", &Structure_Save)) return false;
	if (!Save_Chunk(fp, "MAP ", &Map_Save)) return false;
	if (!Save_Chunk(fp, "TEAM", &Team_Save)) return false;
	if (!Save_Chunk(fp, "ODUN", &UnitNew_Save)) return false;

	/* Write the total length of all data in the FORM chunk */
	long end = ftell(fp);
	if (end < 8 || fseek(fp, 4, SEEK_SET) != 0) return false;
	length = end - 8;
	lengthSwapped = HTOBE32(length);
	if (fwrite(&lengthSwapped, 4, 1, fp) != 1) return false;
	return fseek(fp, end, SEEK_SET) == 0;
}

#ifdef __3DS__
bool SaveGame_WriteStream(FILE *fp, char *description)
{
	g_validateStrictIfZero++;
	bool saved = Save_Main(fp, description);
	g_validateStrictIfZero--;
	return saved;
}

static void SaveGame_MakePaths(const char *filename, char path[1024], char backup[1028])
{
	File_GetPersonalPath(path, 1024, filename);
	snprintf(backup, 1028, "%s.BAK", path);
}

void SaveGame_RecoverFile(const char *filename)
{
	char path[1024], backup[1028];
	struct stat info;

	SaveGame_MakePaths(filename, path, backup);
	/* A previous session may have stopped between the two FAT renames. */
	if (stat(path, &info) != 0 && errno == ENOENT) rename(backup, path);
}

static bool SaveGame_Commit(const char *filename, char *description)
{
	char path[1024], backup[1028], pending[1028];
	struct stat info;

	SaveGame_MakePaths(filename, path, backup);
	snprintf(pending, sizeof(pending), "%s.NEW", path);

	FILE *fp = fopen(pending, "wb");
	if (fp == NULL) return false;

	bool saved = SaveGame_WriteStream(fp, description);
	if (fclose(fp) != 0) saved = false;

	bool movedOld = false;
	if (saved && stat(path, &info) == 0) {
		/* The SD filesystem may refuse to rename over an existing file. */
		saved = (remove(backup) == 0 || errno == ENOENT) && rename(path, backup) == 0;
		movedOld = saved;
	} else if (saved && errno != ENOENT) {
		saved = false;
	}

	if (saved && rename(pending, path) == 0) return true;

	if (movedOld) rename(backup, path);
	remove(pending);
	return false;
}
#endif

/**
 * Save the game to a filename
 *
 * @param fp The filename of the savegame.
 * @param description The description of the savegame.
 * @return True if and only if all bytes were written successful.
 */
bool SaveGame_SaveFile(char *filename, char *description)
{
	/* In debug-scenario mode, the whole map is uncovered. Cover it now in
	 *  the savegame based on the current position of the units and
	 *  structures. */
	if (g_debugScenario) {
		PoolFindStruct find;
		uint16 i;

		/* Add fog of war for all tiles on the map */
		for (i = 0; i < 0x1000; i++) {
			Tile *tile = &g_map[i];
			tile->isUnveiled = false;
			tile->overlayTileID = g_veiledTileID;
		}

		find.houseID = HOUSE_INVALID;
		find.type    = 0xFFFF;
		find.index   = 0xFFFF;

		/* Remove the fog of war for all units */
		while (true) {
			Unit *u;

			u = Unit_Find(&find);
			if (u == NULL) break;

			Unit_RemoveFog(u);
		}

		find.houseID = HOUSE_INVALID;
		find.type    = 0xFFFF;
		find.index   = 0xFFFF;

		/* Remove the fog of war for all structures */
		while (true) {
			Structure *s;

			s = Structure_Find(&find);
			if (s == NULL) break;
			if (s->o.type == STRUCTURE_SLAB_1x1 || s->o.type == STRUCTURE_SLAB_2x2 || s->o.type == STRUCTURE_WALL) continue;

			Structure_RemoveFog(s);
		}
	}

#ifdef __3DS__
	if (SaveGame_Commit(filename, description)) return true;
	Error("Error while writing savegame '%s'.\n", filename);
	return false;
#else
	FILE *fp = fopendatadir(SEARCHDIR_PERSONAL_DATA_DIR, filename, "wb");
	if (fp == NULL) {
		Error("Failed to open file '%s' for writing.\n", filename);
		return false;
	}

	g_validateStrictIfZero++;
	bool res = Save_Main(fp, description);
	g_validateStrictIfZero--;

	if (fclose(fp) != 0) res = false;

	if (!res) {
		/* TODO -- Also remove the savegame now */

		Error("Error while writing savegame.\n");
		return false;
	}

	return true;
#endif
}
