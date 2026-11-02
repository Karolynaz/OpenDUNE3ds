#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "../src/file_3ds.h"
static void Put(const char *path) { FILE *f=fopen(path,"wb"); assert(f); fputc(1,f); fclose(f); }
int main(void)
{
 /* Runs in an isolated temporary working directory supplied by test_3ds.sh. */
 assert(mkdir("sdmc:",0700)==0); assert(mkdir("sdmc:/3ds",0700)==0);
 assert(mkdir("sdmc:/3ds/dune3ds",0700)==0); assert(mkdir("sdmc:/3ds/dune3ds/data",0700)==0);
 assert(mkdir("romfs:",0700)==0); assert(mkdir("romfs:/data",0700)==0);
 Put("romfs:/data/DUNE.PAK");
 assert(strcmp(File_3DS_FindDataDirectory(),"romfs:/data")==0); /* Empty saves/data must not mask ROMFS. */
 Put("sdmc:/3ds/dune3ds/ICON.ICN");
 assert(strcmp(File_3DS_FindDataDirectory(),"sdmc:/3ds/dune3ds")==0);
 Put("sdmc:/3ds/dune3ds/data/dune.pak");
 assert(strcmp(File_3DS_FindDataDirectory(),"sdmc:/3ds/dune3ds/data")==0);
 assert(remove("sdmc:/3ds/dune3ds/data/dune.pak")==0);
 assert(remove("sdmc:/3ds/dune3ds/ICON.ICN")==0);
 assert(remove("romfs:/data/DUNE.PAK")==0);
 Put("romfs:/icon.icn");
 assert(strcmp(File_3DS_FindDataDirectory(),"romfs:")==0);
 assert(remove("romfs:/icon.icn")==0);
 assert(strcmp(File_3DS_FindDataDirectory(),"sdmc:/3ds/dune3ds/data")==0);
 puts("3DS SD and ROMFS path checks passed");
}
