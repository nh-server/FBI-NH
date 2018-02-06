#include <stdlib.h>
#include <string.h>

#include <3ds.h>

#include "error.h"
#include "linkedlist.h"
#include "util.h"
#include "task/task.h"

FS_Path* util_make_path_utf8(const char* path) {
    size_t len = strlen(path);

    u16* utf16 = (u16*) calloc(len + 1, sizeof(u16));
    if(utf16 == NULL) {
        return NULL;
    }

    ssize_t utf16Len = utf8_to_utf16(utf16, (const uint8_t*) path, len);

    FS_Path* fsPath = (FS_Path*) calloc(1, sizeof(FS_Path));
    if(fsPath == NULL) {
        free(utf16);
        return NULL;
    }

    fsPath->type = PATH_UTF16;
    fsPath->size = (utf16Len + 1) * sizeof(u16);
    fsPath->data = utf16;

    return fsPath;
}

void util_free_path_utf8(FS_Path* path) {
    free((void*) path->data);
    free(path);
}

FS_Path util_make_binary_path(const void* data, u32 size) {
    FS_Path path = {PATH_BINARY, size, data};
    return path;
}

bool util_is_dir(FS_Archive archive, const char* path) {
    Result res = 0;

    FS_Path* fsPath = util_make_path_utf8(path);
    if(fsPath != NULL) {
        Handle dirHandle = 0;
        if(R_SUCCEEDED(res = FSUSER_OpenDirectory(&dirHandle, archive, *fsPath))) {
            FSDIR_Close(dirHandle);
        }

        util_free_path_utf8(fsPath);
    } else {
        res = R_APP_OUT_OF_MEMORY;
    }

    return R_SUCCEEDED(res);
}

Result util_ensure_dir(FS_Archive archive, const char* path) {
    Result res = 0;

    FS_Path* fsPath = util_make_path_utf8(path);
    if(fsPath != NULL) {
        Handle dirHandle = 0;
        if(R_SUCCEEDED(FSUSER_OpenDirectory(&dirHandle, archive, *fsPath))) {
            FSDIR_Close(dirHandle);
        } else {
            FSUSER_DeleteFile(archive, *fsPath);
            res = FSUSER_CreateDirectory(archive, *fsPath, 0);
        }

        util_free_path_utf8(fsPath);
    } else {
        res = R_APP_OUT_OF_MEMORY;
    }

    return res;
}

void util_get_file_name(char* out, const char* file, u32 size) {
    const char* end = file + strlen(file);
    const char* curr = file - 1;
    while((curr = strchr(curr + 1, '.')) != NULL) {
        end = curr;
    }

    u32 terminatorPos = end - file < size - 1 ? end - file : size - 1;
    strncpy(out, file, terminatorPos);
    out[terminatorPos] = '\0';
}

void util_get_path_file(char* out, const char* path, u32 size) {
    const char* start = NULL;
    const char* end = NULL;
    const char* curr = path - 1;
    while((curr = strchr(curr + 1, '/')) != NULL) {
        start = end != NULL ? end : path;
        end = curr;
    }

    if(end != path + strlen(path) - 1) {
        start = end;
        end = path + strlen(path);
    }

    if(end - start == 0) {
        strncpy(out, "/", size);
    } else {
        u32 terminatorPos = end - start - 1 < size - 1 ? end - start - 1 : size - 1;
        strncpy(out, start + 1, terminatorPos);
        out[terminatorPos] = '\0';
    }
}

void util_get_parent_path(char* out, const char* path, u32 size) {
    size_t pathLen = strlen(path);

    const char* start = NULL;
    const char* end = NULL;
    const char* curr = path - 1;
    while((curr = strchr(curr + 1, '/')) != NULL && (start == NULL || curr != path + pathLen - 1)) {
        start = end != NULL ? end : path;
        end = curr;
    }

    u32 terminatorPos = end - path + 1 < size - 1 ? end - path + 1 : size - 1;
    strncpy(out, path, terminatorPos);
    out[terminatorPos] = '\0';
}

bool util_is_string_empty(const char* str) {
    if(strlen(str) == 0) {
        return true;
    }

    const char* curr = str;
    while(*curr) {
        if(*curr != ' ') {
            return false;
        }

        curr++;
    }

    return true;
}

FS_MediaType util_get_title_destination(u64 titleId) {
    u16 platform = (u16) ((titleId >> 48) & 0xFFFF);
    u16 category = (u16) ((titleId >> 32) & 0xFFFF);
    u8 variation = (u8) (titleId & 0xFF);

    //     DSiWare                3DS                    DSiWare, System, DLP         Application           System Title
    return platform == 0x0003 || (platform == 0x0004 && ((category & 0x8011) != 0 || (category == 0x0000 && variation == 0x02))) ? MEDIATYPE_NAND : MEDIATYPE_SD;
}

bool util_filter_cias(void* data, const char* name, u32 attributes) {
    if((attributes & FS_ATTRIBUTE_DIRECTORY) != 0) {
        return false;
    }

    size_t len = strlen(name);
    return len >= 4 && strncasecmp(name + len - 4, ".cia", 4) == 0;
}

bool util_filter_tickets(void* data, const char* name, u32 attributes) {
    if((attributes & FS_ATTRIBUTE_DIRECTORY) != 0) {
        return false;
    }

    size_t len = strlen(name);
    return (len >= 4 && strncasecmp(name + len - 4, ".tik", 4) == 0) || (len >= 5 && strncasecmp(name + len - 5, ".cetk", 5) == 0);
}

static char path_3dsx[FILE_PATH_MAX] = {'\0'};

const char* util_get_3dsx_path() {
    if(strlen(path_3dsx) == 0) {
        return NULL;
    }

    return path_3dsx;
}

void util_set_3dsx_path(const char* path) {
    if(strlen(path) >= 5 && strncmp(path, "sdmc:", 5) == 0) {
        strncpy(path_3dsx, path + 5, FILE_PATH_MAX);
    } else {
        strncpy(path_3dsx, path, FILE_PATH_MAX);
    }
}

typedef struct {
    FS_Archive archive;
    u32 refs;
} archive_ref;

static linked_list opened_archives;

Result util_open_archive(FS_Archive* archive, FS_ArchiveID id, FS_Path path) {
    if(archive == NULL) {
        return R_APP_INVALID_ARGUMENT;
    }

    Result res = 0;

    FS_Archive arch = 0;
    if(R_SUCCEEDED(res = FSUSER_OpenArchive(&arch, id, path))) {
        if(R_SUCCEEDED(res = util_ref_archive(arch))) {
            *archive = arch;
        } else {
            FSUSER_CloseArchive(arch);
        }
    }

    return res;
}

Result util_ref_archive(FS_Archive archive) {
    linked_list_iter iter;
    linked_list_iterate(&opened_archives, &iter);

    while(linked_list_iter_has_next(&iter)) {
        archive_ref* ref = (archive_ref*) linked_list_iter_next(&iter);
        if(ref->archive == archive) {
            ref->refs++;
            return 0;
        }
    }

    Result res = 0;

    archive_ref* ref = (archive_ref*) calloc(1, sizeof(archive_ref));
    if(ref != NULL) {
        ref->archive = archive;
        ref->refs = 1;

        linked_list_add(&opened_archives, ref);
    } else {
        res = R_APP_OUT_OF_MEMORY;
    }

    return res;
}

Result util_close_archive(FS_Archive archive) {
    linked_list_iter iter;
    linked_list_iterate(&opened_archives, &iter);

    while(linked_list_iter_has_next(&iter)) {
        archive_ref* ref = (archive_ref*) linked_list_iter_next(&iter);
        if(ref->archive == archive) {
            ref->refs--;

            if(ref->refs == 0) {
                linked_list_iter_remove(&iter);
                free(ref);
            } else {
                return 0;
            }
        }
    }

    return FSUSER_CloseArchive(archive);
}

void util_escape_file_name(char* out, const char* file, size_t size) {
    static const char reservedChars[] = {'<', '>', ':', '"', '/', '\\', '|', '?', '*'};

    for(u32 i = 0; i < size; i++) {
        bool reserved = false;
        for(u32 j = 0; j < sizeof(reservedChars); j++) {
            if(file[i] == reservedChars[j]) {
                reserved = true;
                break;
            }
        }

        if(reserved) {
            out[i] = '_';
        } else {
            out[i] = file[i];
        }

        if(file[i] == '\0') {
            break;
        }
    }
}
