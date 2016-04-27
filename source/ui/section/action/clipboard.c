#include <malloc.h>
#include <string.h>

#include <3ds.h>

#include "clipboard.h"
#include "../task/task.h"

static bool clipboard_has = false;
static bool clipboard_contents_only;
static FS_Archive clipboard_archive;
static void* clipboard_archive_path;
static char clipboard_path[FILE_PATH_MAX];

bool clipboard_has_contents() {
    return clipboard_has;
}

FS_Archive* clipboard_get_archive() {
    return &clipboard_archive;
}

char* clipboard_get_path() {
    return clipboard_path;
}

bool clipboard_is_contents_only() {
    return clipboard_contents_only;
}

Result clipboard_set_contents(FS_Archive archive, const char* path, bool contentsOnly) {
    clipboard_clear();

    clipboard_has = true;
    clipboard_contents_only = contentsOnly;
    clipboard_archive = archive;
    strncpy(clipboard_path, path, FILE_PATH_MAX);

    if(clipboard_archive.lowPath.size > 0) {
        clipboard_archive_path = calloc(1, clipboard_archive.lowPath.size);
        if(clipboard_archive_path == NULL) {
            clipboard_clear();
            return R_FBI_OUT_OF_MEMORY;
        }

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
    clipboard_contents_only = false;
    memset(clipboard_path, '\0', FILE_PATH_MAX);
}