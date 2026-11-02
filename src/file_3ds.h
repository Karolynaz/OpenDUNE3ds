/* 3DS data-directory probing is separate from the writable save directory. */
#ifndef FILE_3DS_H
#define FILE_3DS_H

#include <stdio.h>

static const char *File_3DS_FindDataDirectory(void)
{
	static const char *directories[] = {
		"sdmc:/3ds/dune3ds/data", "sdmc:/3ds/dune3ds", "romfs:/data", "romfs:"
	};
	/* Accept the original archive or an unpacked core graphics resource.
	 * Probe both cases because ROMFS paths are case sensitive. */
	static const char *markers[] = {"DUNE.PAK", "dune.pak", "ICON.ICN", "icon.icn"};
	char path[128];
	for (unsigned i = 0; i < sizeof(directories) / sizeof(directories[0]); i++) {
		for (unsigned j = 0; j < sizeof(markers) / sizeof(markers[0]); j++) {
			snprintf(path, sizeof(path), "%s/%s", directories[i], markers[j]);
			FILE *file = fopen(path, "rb");
			if (file == NULL) continue;
			int firstByte = fgetc(file);
			fclose(file);
			if (firstByte != EOF) return directories[i];
		}
	}
	return directories[0];
}
#endif
