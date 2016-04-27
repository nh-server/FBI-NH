#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <3ds.h>

#include "util.h"
#include "../ui/section/task/task.h"

extern void cleanup();

static int util_get_line_length(PrintConsole* console, const char* str) {
    int lineLength = 0;
    while(*str != 0) {
        if(*str == '\n') {
            break;
        }

        lineLength++;
        if(lineLength >= console->consoleWidth - 1) {
            break;
        }

        str++;
    }

    return lineLength;
}

static int util_get_lines(PrintConsole* console, const char* str) {
    int lines = 1;
    int lineLength = 0;
    while(*str != 0) {
        if(*str == '\n') {
            lines++;
            lineLength = 0;
        } else {
            lineLength++;
            if(lineLength >= console->consoleWidth - 1) {
                lines++;
                lineLength = 0;
            }
        }

        str++;
    }

    return lines;
}

void util_panic(const char* s, ...) {
    va_list list;
    va_start(list, s);

    char buf[1024];
    vsnprintf(buf, 1024, s, list);

    va_end(list);

    gspWaitForVBlank();

    u16 width;
    u16 height;
    for(int i = 0; i < 2; i++) {
        memset(gfxGetFramebuffer(GFX_TOP, GFX_LEFT, &width, &height), 0, (size_t) (width * height * 3));
        memset(gfxGetFramebuffer(GFX_TOP, GFX_RIGHT, &width, &height), 0, (size_t) (width * height * 3));
        memset(gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, &width, &height), 0, (size_t) (width * height * 3));

        gfxSwapBuffers();
    }

    PrintConsole* console = consoleInit(GFX_TOP, NULL);

    const char* header = "FBI has encountered a fatal error!";
    const char* footer = "Press any button to exit.";

    printf("\x1b[0;0H");
    for(int i = 0; i < console->consoleWidth; i++) {
        printf("-");
    }

    printf("\x1b[%d;0H", console->consoleHeight - 1);
    for(int i = 0; i < console->consoleWidth; i++) {
        printf("-");
    }

    printf("\x1b[0;%dH%s", (console->consoleWidth - util_get_line_length(console, header)) / 2, header);
    printf("\x1b[%d;%dH%s", console->consoleHeight - 1, (console->consoleWidth - util_get_line_length(console, footer)) / 2, footer);

    int bufRow = (console->consoleHeight - util_get_lines(console, buf)) / 2;
    char* str = buf;
    while(*str != 0) {
        if(*str == '\n') {
            bufRow++;
            str++;
            continue;
        } else {
            int lineLength = util_get_line_length(console, str);

            char old = *(str + lineLength);
            *(str + lineLength) = '\0';
            printf("\x1b[%d;%dH%s", bufRow, (console->consoleWidth - lineLength) / 2, str);
            *(str + lineLength) = old;

            bufRow++;
            str += lineLength;
        }
    }

    gfxFlushBuffers();
    gspWaitForVBlank();

    while(aptMainLoop()) {
        hidScanInput();
        if(hidKeysDown() & ~KEY_TOUCH) {
            break;
        }

        gspWaitForVBlank();
    }

    cleanup();
    exit(1);
}

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

bool util_exists(FS_Archive* archive, const char* path) {
    bool exists = false;

    FS_Path* fsPath = util_make_path_utf8(path);
    if(path != NULL) {
        Handle handle = 0;
        if(R_SUCCEEDED(FSUSER_OpenFile(&handle, *archive, *fsPath, FS_OPEN_READ, 0))) {
            FSFILE_Close(handle);
            exists = true;
        } else if(R_SUCCEEDED(FSUSER_OpenDirectory(&handle, *archive, *fsPath))) {
            FSDIR_Close(handle);
            exists = true;
        }

        util_free_path_utf8(fsPath);
    }

    return exists;
}

bool util_is_dir(FS_Archive* archive, const char* path) {
    Result res = 0;

    FS_Path* fsPath = util_make_path_utf8(path);
    if(fsPath != NULL) {
        Handle dirHandle = 0;
        if(R_SUCCEEDED(res = FSUSER_OpenDirectory(&dirHandle, *archive, *fsPath))) {
            FSDIR_Close(dirHandle);
        }

        util_free_path_utf8(fsPath);
    } else {
        res = R_FBI_OUT_OF_MEMORY;
    }

    return R_SUCCEEDED(res);
}

Result util_ensure_dir(FS_Archive* archive, const char* path) {
    Result res = 0;

    if(!util_is_dir(archive, path)) {
        FS_Path* fsPath = util_make_path_utf8(path);
        if(fsPath != NULL) {
            FSUSER_DeleteFile(*archive, *fsPath);
            res = FSUSER_CreateDirectory(*archive, *fsPath, 0);

            util_free_path_utf8(fsPath);
        } else {
            res = R_FBI_OUT_OF_MEMORY;
        }
    }

    return res;
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

bool util_filter_dirs(void* data, FS_Archive* archive, const char* path, u32 attributes) {
    return (bool) (attributes & FS_ATTRIBUTE_DIRECTORY);
}

bool util_filter_files(void* data, FS_Archive* archive, const char* path, u32 attributes) {
    return !(attributes & FS_ATTRIBUTE_DIRECTORY);
}

bool util_filter_hidden(void* data, FS_Archive* archive, const char* path, u32 attributes) {
    return !(attributes & FS_ATTRIBUTE_HIDDEN);
}

bool util_filter_file_extension(void* data, FS_Archive* archive, const char* path, u32 attributes) {
    if(data == NULL) {
        return true;
    }

    char* extension = (char*) data;
    size_t extensionLen = strlen(extension);

    size_t len = strlen(path);
    return util_filter_files(data, archive, path, attributes) && len >= extensionLen && strcmp(path + len - extensionLen, extension) == 0;
}

bool util_filter_not_path(void* data, FS_Archive* archive, const char* path, u32 attributes) {
    if(data == NULL) {
        return true;
    }

    return strcmp(path, (char*) data) != 0;
}

static Result util_traverse_dir_internal(FS_Archive* archive, const char* path, bool recursive, bool dirsFirst, void* data, bool (*filter)(void* data, FS_Archive* archive, const char* path, u32 attributes),
                                                                                                                            void (*process)(void* data, FS_Archive* archive, const char* path, u32 attributes)) {
    Result res = 0;

    FS_Path* fsPath = util_make_path_utf8(path);
    if(fsPath != NULL) {
        Handle handle = 0;
        if(R_SUCCEEDED(res = FSUSER_OpenDirectory(&handle, *archive, *fsPath))) {
            size_t pathLen = strlen(path);
            char* pathBuf = (char*) calloc(1, FILE_PATH_MAX);
            if(pathBuf != NULL) {
                strncpy(pathBuf, path, FILE_PATH_MAX);

                u32 entryCount = 0;
                FS_DirectoryEntry entry;
                u32 done = 0;
                while(R_SUCCEEDED(FSDIR_Read(handle, &entryCount, 1, &entry)) && entryCount > 0) {
                    ssize_t units = utf16_to_utf8((uint8_t*) pathBuf + pathLen, entry.name, FILE_PATH_MAX - pathLen - 1);
                    if(units > 0) {
                        pathBuf[pathLen + units] = '\0';
                        if(entry.attributes & FS_ATTRIBUTE_DIRECTORY) {
                            if(pathLen + units < FILE_PATH_MAX - 2) {
                                pathBuf[pathLen + units] = '/';
                                pathBuf[pathLen + units + 1] = '\0';
                            }
                        }

                        if(dirsFirst) {
                            if(process != NULL && (filter == NULL || filter(data, archive, pathBuf, entry.attributes))) {
                                process(data, archive, pathBuf, entry.attributes);
                            }
                        }

                        if((entry.attributes & FS_ATTRIBUTE_DIRECTORY) && recursive) {
                            if(R_FAILED(res = util_traverse_dir_internal(archive, pathBuf, recursive, dirsFirst, data, filter, process))) {
                                break;
                            }
                        }

                        if(!dirsFirst) {
                            if(process != NULL && (filter == NULL || filter(data, archive, pathBuf, entry.attributes))) {
                                process(data, archive, pathBuf, entry.attributes);
                            }
                        }
                    }

                    done++;
                }

                free(pathBuf);
            } else {
                res = R_FBI_OUT_OF_MEMORY;
            }

            FSDIR_Close(handle);
        }

        util_free_path_utf8(fsPath);
    } else {
        res = R_FBI_OUT_OF_MEMORY;
    }

    return res;
}

static Result util_traverse_dir(FS_Archive* archive, const char* path, bool recursive, bool dirsFirst, void* data, bool (*filter)(void* data, FS_Archive* archive, const char* path, u32 attributes),
                                                                                                                   void (*process)(void* data, FS_Archive* archive, const char* path, u32 attributes)) {
    if(dirsFirst && strcmp(path, "/") != 0) {
        if(process != NULL && (filter == NULL || filter(data, archive, path, FS_ATTRIBUTE_DIRECTORY))) {
            process(data, archive, path, FS_ATTRIBUTE_DIRECTORY);
        }
    }

    Result res = util_traverse_dir_internal(archive, path, recursive, dirsFirst, data, filter, process);

    if(!dirsFirst && strcmp(path, "/") != 0) {
        if(process != NULL && (filter == NULL || filter(data, archive, path, FS_ATTRIBUTE_DIRECTORY))) {
            process(data, archive, path, FS_ATTRIBUTE_DIRECTORY);
        }
    }

    return res;
}

static Result util_traverse_file(FS_Archive* archive, const char* path, bool recursive, bool dirsFirst, void* data, bool (*filter)(void* data, FS_Archive* archive, const char* path, u32 attributes),
                                                                                                                    void (*process)(void* data, FS_Archive* archive, const char* path, u32 attributes)) {
    Result res = 0;

    FS_Path* fsPath = util_make_path_utf8(path);
    if(fsPath != NULL) {
        Handle handle = 0;
        if(R_SUCCEEDED(res = FSUSER_OpenFile(&handle, *archive, *fsPath, FS_OPEN_READ, 0))) {
            if(process != NULL && (filter == NULL || filter(data, archive, path, 0))) {
                process(data, archive, path, 0);
            }

            FSFILE_Close(handle);
        }

        util_free_path_utf8(fsPath);
    } else {
        res = R_FBI_OUT_OF_MEMORY;
    }

    return res;
}

Result util_traverse_contents(FS_Archive* archive, const char* path, bool recursive, bool dirsFirst, void* data, bool (*filter)(void* data, FS_Archive* archive, const char* path, u32 attributes),
                                                                                                                 void (*process)(void* data, FS_Archive* archive, const char* path, u32 attributes)) {
    Result res = 0;

    if(util_is_dir(archive, path)) {
        res = util_traverse_dir(archive, path, recursive, dirsFirst, data, filter, process);
    } else {
        res = util_traverse_file(archive, path, recursive, dirsFirst, data, filter, process);
    }

    return res;
}

typedef struct {
    u32* count;
    void* data;
    bool (*filter)(void* data, FS_Archive* archive, const char* path, u32 attributes);
} count_data;

static bool util_count_contents_filter(void* data, FS_Archive* archive, const char* path, u32 attributes) {
    count_data* countData = (count_data*) data;
    if(countData->filter != NULL) {
        return countData->filter(countData->data, archive, path, attributes);
    }

    return true;
}

static void util_count_contents_process(void* data, FS_Archive* archive, const char* path, u32 attributes) {
    (*((count_data*) data)->count)++;
}

Result util_count_contents(u32* out, FS_Archive* archive, const char* path, bool recursive, bool dirsFirst, void* data, bool (*filter)(void* data, FS_Archive* archive, const char* path, u32 attributes)) {
    if(out == NULL) {
        return 0;
    }

    count_data countData;
    countData.count = out;
    countData.data = data;
    countData.filter = filter;
    return util_traverse_contents(archive, path, recursive, dirsFirst, &countData, util_count_contents_filter, util_count_contents_process);
}

typedef struct {
    char*** contents;
    u32 index;
    void* data;
    bool (*filter)(void* data, FS_Archive* archive, const char* path, u32 attributes);
} populate_data;

static bool util_populate_contents_filter(void* data, FS_Archive* archive, const char* path, u32 attributes) {
    populate_data* populateData = (populate_data*) data;
    if(populateData->filter != NULL) {
        return populateData->filter(populateData->data, archive, path, attributes);
    }

    return true;
}

static void util_populate_contents_process(void* data, FS_Archive* archive, const char* path, u32 attributes) {
    u32 currPathSize = strlen(path) + 1;
    char* currPath = (char*) calloc(1, currPathSize);
    if(currPath == NULL) {
        return;
    }

    strncpy(currPath, path, currPathSize);

    populate_data* populateData = (populate_data*) data;
    (*populateData->contents)[populateData->index++] = currPath;
}

Result util_populate_contents(char*** contentsOut, u32* countOut, FS_Archive* archive, const char* path, bool recursive, bool dirsFirst, void* data, bool (*filter)(void* data, FS_Archive* archive, const char* path, u32 attributes)) {
    if(contentsOut == NULL || countOut == NULL) {
        return 0;
    }

    util_count_contents(countOut, archive, path, recursive, dirsFirst, data, filter);
    *contentsOut = (char**) calloc(*countOut, sizeof(char*));

    if(*contentsOut == NULL) {
        return R_FBI_OUT_OF_MEMORY;
    }

    populate_data populateData;
    populateData.contents = contentsOut;
    populateData.index = 0;
    populateData.data = data;
    populateData.filter = filter;

    Result res = util_traverse_contents(archive, path, recursive, dirsFirst, &populateData, util_populate_contents_filter, util_populate_contents_process);
    if(R_FAILED(res)) {
        util_free_contents(*contentsOut, *countOut);
    }

    return res;
}

void util_free_contents(char** contents, u32 count) {
    for(u32 i = 0; i < count; i++) {
        if(contents[i] != NULL) {
            free(contents[i]);
        }
    }

    free(contents);
}

int util_compare_u32(const void* e1, const void* e2) {
    u32 id1 = *(u32*) e1;
    u32 id2 = *(u32*) e2;

    return id1 > id2 ? 1 : id1 < id2 ? -1 : 0;
}

int util_compare_u64(const void* e1, const void* e2) {
    u64 id1 = *(u64*) e1;
    u64 id2 = *(u64*) e2;

    return id1 > id2 ? 1 : id1 < id2 ? -1 : 0;
}

int util_compare_directory_entries(const void* e1, const void* e2) {
    FS_DirectoryEntry* ent1 = (FS_DirectoryEntry*) e1;
    FS_DirectoryEntry* ent2 = (FS_DirectoryEntry*) e2;

    if((ent1->attributes & FS_ATTRIBUTE_DIRECTORY) && !(ent2->attributes & FS_ATTRIBUTE_DIRECTORY)) {
        return -1;
    } else if(!(ent1->attributes & FS_ATTRIBUTE_DIRECTORY) && (ent2->attributes & FS_ATTRIBUTE_DIRECTORY)) {
        return 1;
    } else {
        char entryName1[0x213] = {'\0'};
        utf16_to_utf8((uint8_t*) entryName1, ent1->name, sizeof(entryName1) - 1);

        char entryName2[0x213] = {'\0'};
        utf16_to_utf8((uint8_t*) entryName2, ent2->name, sizeof(entryName2) - 1);

        return strcasecmp(entryName1, entryName2);
    }
}