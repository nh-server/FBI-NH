#include <malloc.h>
#include <string.h>

#include <3ds.h>

#include "clipboard.h"
#include "../task/task.h"

static bool clipboard_has = false;
static bool clipboard_contents_only;

static FS_Archive clipboard_archive;
static char clipboard_path[FILE_PATH_MAX];

bool clipboard_has_contents() {
    return clipboard_has;
}

FS_Archive clipboard_get_archive() {
    return clipboard_archive;
}

char* clipboard_get_path() {
    return clipboard_path;
}

bool clipboard_is_contents_only() {
    return clipboard_contents_only;
}

Result clipboard_set_contents(FS_ArchiveID archiveId, FS_Path* archivePath, const char* path, bool contentsOnly) {
    clipboard_clear();

    clipboard_has = true;
    clipboard_contents_only = contentsOnly;

    strncpy(clipboard_path, path, FILE_PATH_MAX);

    Result res = 0;
    if(R_FAILED(res = FSUSER_OpenArchive(&clipboard_archive, archiveId, *archivePath))) {
        clipboard_clear();
    }

    return res;
}

void clipboard_clear() {
    clipboard_has = false;
    clipboard_contents_only = false;

    memset(clipboard_path, '\0', FILE_PATH_MAX);

    if(clipboard_archive != 0) {
        FSUSER_CloseArchive(clipboard_archive);
        clipboard_archive = 0;
    }
}