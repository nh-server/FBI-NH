#include <sys/syslimits.h>
#include <malloc.h>
#include <string.h>

#include <3ds.h>
#include <3ds/services/fs.h>

#include "clipboard.h"

static bool clipboard_has = false;
static FS_Archive clipboard_archive;
static void* clipboard_archive_path;
static char clipboard_path[PATH_MAX];

bool clipboard_has_contents() {
    return clipboard_has;
}

FS_Archive* clipboard_get_archive() {
    return &clipboard_archive;
}

char* clipboard_get_path() {
    return clipboard_path;
}

Result clipboard_set_contents(FS_Archive archive, const char* path) {
    clipboard_clear();

    clipboard_has = true;
    clipboard_archive = archive;
    strncpy(clipboard_path, path, PATH_MAX);

    if(clipboard_archive.lowPath.size > 0) {
        clipboard_archive_path = calloc(1, clipboard_archive.lowPath.size);
        memcpy(clipboard_archive_path, clipboard_archive.lowPath.data, clipboard_archive.lowPath.size);
        clipboard_archive.lowPath.data = clipboard_archive_path;
    }

    clipboard_archive.handle = 0;
    return FSUSER_OpenArchive(&clipboard_archive);
}

void clipboard_clear() {
    if(clipboard_archive.handle != 0) {
        FSUSER_CloseArchive(&clipboard_archive);
        clipboard_archive.handle = 0;
    }

    if(clipboard_archive_path != NULL) {
        free(clipboard_archive_path);
        clipboard_archive_path = NULL;
    }

    clipboard_has = false;
    clipboard_path[0] = '\0';
}