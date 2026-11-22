/** @file src/load.c Load routines. */

#include <stdio.h>
#include <stdlib.h>
#include "multichar.h"
#include "types.h"
#include "os/endian.h"
#include "os/strings.h"

#include "load.h"

#include "audio/sound.h"
#include "config.h"
#include "file.h"
#include "gui/gui.h"
#include "gui/widget.h"
#include "house.h"
#include "map.h"
#include "opendune.h"
#include "os/error.h"
#include "saveload/saveload.h"
#ifdef __3DS__
#include "save.h"
#endif
#include "sprites.h"
#include "string.h"
#include "structure.h"
#include "table/strings.h"
#include "team.h"
#include "unit.h"


static uint32 Load_FindChunk(FILE *fp, uint32 chunk)
{
	uint32 header;
	uint32 length;

	while (fread(&header, sizeof(uint32), 1, fp) == 1) {
		if (fread(&length, sizeof(uint32), 1, fp) != 1) return 0;
		length = BETOH32(length);
		if (BETOH32(header) != chunk) {
			fseek(fp, length + (length & 1), SEEK_CUR);
			continue;
		}
		return length;
	}
	return 0;
}

static bool Load_Main(FILE *fp)
{
	uint32 position;
	uint32 length;
	uint32 header;
	uint16 version;

	/* All OpenDUNE / Dune2 savegames should start with 'FORM' */
	if (fread(&header, sizeof(uint32), 1, fp) != 1) return false;
	if (BETOH32(header) != CC_FORM) {
		Error("Invalid magic header in savegame. Not an OpenDUNE / Dune2 savegame.");
		return false;
	}

	/* The total length field, which is ignored */
	if (fread(&length, sizeof(uint32), 1, fp) != 1) return false;

	/* The next 'chunk' is fake, and has no length field */
	if (fread(&header, sizeof(uint32), 1, fp) != 1) return false;
	if (BETOH32(header) != CC_SCEN) return false;

	position = ftell(fp);

	/* Find the 'INFO' chunk, as it contains the savegame version */
	version = 0;
	length = Load_FindChunk(fp, CC_INFO);
	if (length == 0) return false;

	/* Read the savegame version */
	if (!fread_le_uint16(&version, fp)) return false;
	length -= 2;
	if (version == 0) return false;

	if (version != 0x0290) {
		/* Get the scenarioID / campaignID */
		if (!Info_LoadOld(fp, length)) return false;

		g_gameMode = GM_RESTART;

		/* Find the 'PLYR' chunk */
		fseek(fp, position, SEEK_SET);
		length = Load_FindChunk(fp, CC_PLYR);
		if (length == 0) return false;

		/* Find the human player */
		if (!House_LoadOld(fp, length)) return false;

		GUI_DisplayModalMessage(String_Get_ByIndex(STR_WARNING_ORIGINAL_SAVED_GAMES_ARE_INCOMPATABLE_WITH_THE_NEW_VERSION_THE_BATTLE_WILL_BE_RESTARTED), 0xFFFF);

		return true;
	}

	/* Load the 'INFO' chunk'. It has to be the first chunk loaded */
	if (!Info_Load(fp, length)) return false;

	/* Rewind, and read other chunks */
	fseek(fp, position, SEEK_SET);
	while (fread(&header, sizeof(uint32), 1, fp) == 1) {
		if (fread(&length, sizeof(uint32), 1, fp) != 1) return false;
		length = BETOH32(length);

		switch (BETOH32(header)) {
			case CC_NAME: break; /* 'NAME' chunk is of no interest to us */
			case CC_INFO: break; /* 'INFO' chunk is already read */
			case CC_MAP : if (!Map_Load      (fp, length)) return false; break;
			case CC_PLYR: if (!House_Load    (fp, length)) return false; break;
			case CC_UNIT: if (!Unit_Load     (fp, length)) return false; break;
			case CC_BLDG: if (!Structure_Load(fp, length)) return false; break;
			case CC_TEAM: if (!Team_Load     (fp, length)) return false; break;
			case CC_ODUN: if (!UnitNew_Load  (fp, length)) return false; break;

			default:
				Error("Unknown chunk in savegame: %c%c%c%c (length: %d). Skipped.\n", header, header >> 8, header >> 16, header >> 24, length);
				break;
		}

		/* Savegames are word aligned */
		position += length + 8 + (length & 1);
		fseek(fp, position, SEEK_SET);
	}

	return true;
}

#ifdef __3DS__
static bool Load_CheckLayout(FILE *fp, long size)
{
	uint32 header, length, formLength;
	uint16 version = 0;
	enum {
		INFO = 1, HOUSE = 2, UNITS = 4, BUILDINGS = 8, MAP = 16, TEAMS = 32
	};
	unsigned seen = 0;

	if (fread(&header, 4, 1, fp) != 1 || BETOH32(header) != CC_FORM ||
			fread(&formLength, 4, 1, fp) != 1 ||
			fread(&header, 4, 1, fp) != 1 || BETOH32(header) != CC_SCEN) return false;

	for (long position = 12; position < size;) {
		if (size - position < 8 || fread(&header, 4, 1, fp) != 1 ||
				fread(&length, 4, 1, fp) != 1) return false;

		length = BETOH32(length);
		position += 8;
		if (length > (unsigned long)(size - position) ||
				(length & 1) > (unsigned long)(size - position) - length) return false;

		unsigned field = 0;
		switch (BETOH32(header)) {
			case CC_INFO:
				if (length < 2 || !fread_le_uint16(&version, fp) || version == 0) return false;
				field = INFO;
				break;
			case CC_PLYR: if (length == 0) return false; field = HOUSE; break;
			case CC_UNIT: field = UNITS; break;
			case CC_BLDG: field = BUILDINGS; break;
			case CC_MAP:  field = MAP; break;
			case CC_TEAM: field = TEAMS; break;
			default: break;
		}

		if (seen & field) return false;
		seen |= field;
		position += length + (length & 1);
		if (fseek(fp, position, SEEK_SET) != 0) return false;
	}

	unsigned required = INFO | HOUSE;
	if (version == 0x0290) {
		if (BETOH32(formLength) != (unsigned long)(size - 8)) return false;
		required |= UNITS | BUILDINGS | MAP | TEAMS;
	}
	return (seen & required) == required && fseek(fp, 0, SEEK_SET) == 0;
}
#endif

bool SaveGame_LoadFile(char *filename)
{
#ifdef __3DS__
	SaveGame_RecoverFile(filename);
#endif
	FILE *fp = fopendatadir(SEARCHDIR_PERSONAL_DATA_DIR, filename, "rb");
	if (fp == NULL) {
		Error("Failed to open file '%s' for reading.\n", filename);
		return false;
	}

#ifdef __3DS__
	/* Read the whole file before changing the running game. The SD card is
	 * no longer needed once the load starts. */
	long size = fseek(fp, 0, SEEK_END) == 0 ? ftell(fp) : -1;
	char *data = size >= 12 ? malloc(size) : NULL;
	bool read = data != NULL && fseek(fp, 0, SEEK_SET) == 0 && fread(data, 1, size, fp) == (size_t)size;
	fclose(fp);
	fp = read ? fmemopen(data, size, "rb") : NULL;

	char *backup = NULL;
	size_t backupSize = 0;
	FILE *restore = NULL;
	const GameMode oldMode = g_gameMode;
	bool ready = fp != NULL && Load_CheckLayout(fp, size);
	if (ready && oldMode == GM_NORMAL) {
		FILE *snapshot = open_memstream(&backup, &backupSize);
		ready = snapshot != NULL;
		if (snapshot != NULL) {
			ready = SaveGame_WriteStream(snapshot, "");
			if (fclose(snapshot) != 0) ready = false;
		}
		if (ready && backupSize != 0) restore = fmemopen(backup, backupSize, "rb");
		ready = restore != NULL;
	}

	if (!ready) {
		if (fp != NULL) fclose(fp);
		free(data);
		free(backup);
		Error("Could not prepare savegame '%s' for loading.\n", filename);
		return false;
	}
#endif

	Sound_Output_Feedback(0xFFFE);
	Game_Init();
	Sprites_LoadTiles();

	g_validateStrictIfZero++;
	bool res = Load_Main(fp);
	g_validateStrictIfZero--;

	fclose(fp);

#ifdef __3DS__
	free(data);
	if (!res) {
		g_gameMode = oldMode;
		if (restore != NULL) {
			Game_Init();
			g_validateStrictIfZero++;
			bool restored = Load_Main(restore);
			g_validateStrictIfZero--;
			if (restored) Game_Prepare();
			else Error("Could not restore the running mission.\n");
		}
	}
	if (restore != NULL) fclose(restore);
	free(backup);
#endif

	if (!res) {
		Error("Error while loading savegame '%s'.\n", filename);
		return false;
	}

	if (g_gameMode != GM_RESTART) Game_Prepare();

	return true;
}

/**
 * In case the current house is Mercenary, another palette is loaded.
 */
void Load_Palette_Mercenaries(void)
{
	if (g_playerHouseID == HOUSE_MERCENARY) {
		File_ReadBlockFile("IBM.PAL", g_palette1, 256 * 3);
	}
}
