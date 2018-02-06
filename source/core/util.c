#include <sys/iosupport.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <3ds.h>

#include "linkedlist.h"
#include "util.h"
#include "task/task.h"

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
        res = R_FBI_OUT_OF_MEMORY;
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
        res = R_FBI_OUT_OF_MEMORY;
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
        return R_FBI_INVALID_ARGUMENT;
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
        res = R_FBI_OUT_OF_MEMORY;
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

#define HTTPC_TIMEOUT 15000000000

Result util_http_open(httpcContext* context, const char* url, bool userAgent) {
    return util_http_open_ranged(context, url, userAgent, 0, 0);
}

Result util_http_open_ranged(httpcContext* context, const char* url, bool userAgent, u32 rangeStart, u32 rangeEnd) {
    if(context == NULL || url == NULL) {
        return R_FBI_INVALID_ARGUMENT;
    }

    char currUrl[1024];
    strncpy(currUrl, url, sizeof(currUrl));

    char range[64];
    if(rangeEnd > rangeStart) {
        snprintf(range, sizeof(range), "%lu-%lu", rangeStart, rangeEnd);
    } else {
        snprintf(range, sizeof(range), "%lu-", rangeStart);
    }

    Result res = 0;

    bool resolved = false;
    u32 redirectCount = 0;
    while(R_SUCCEEDED(res) && !resolved && redirectCount < 32) {
        if(R_SUCCEEDED(res = httpcOpenContext(context, HTTPC_METHOD_GET, currUrl, 1))) {
            u32 response = 0;
            if(R_SUCCEEDED(res = httpcSetSSLOpt(context, SSLCOPT_DisableVerify))
               && (!userAgent || R_SUCCEEDED(res = httpcAddRequestHeaderField(context, "User-Agent", HTTP_USER_AGENT)))
               && (rangeStart == 0 || R_SUCCEEDED(res = httpcAddRequestHeaderField(context, "Range", range)))
               && R_SUCCEEDED(res = httpcSetKeepAlive(context, HTTPC_KEEPALIVE_ENABLED))
               && R_SUCCEEDED(res = httpcBeginRequest(context))
               && R_SUCCEEDED(res = httpcGetResponseStatusCodeTimeout(context, &response, HTTPC_TIMEOUT))) {
                if(response == 301 || response == 302 || response == 303) {
                    redirectCount++;

                    memset(currUrl, '\0', sizeof(currUrl));
                    if(R_SUCCEEDED(res = httpcGetResponseHeader(context, "Location", currUrl, sizeof(currUrl)))) {
                        httpcCloseContext(context);
                    }
                } else {
                    resolved = true;

                    if(response != 200) {
                        res = R_FBI_HTTP_ERROR_BASE + response;
                    }
                }
            }

            if(R_FAILED(res)) {
                httpcCloseContext(context);
            }
        }
    }

    if(R_SUCCEEDED(res) && redirectCount >= 32) {
        res = R_FBI_TOO_MANY_REDIRECTS;
    }

    return res;
}

Result util_http_get_size(httpcContext* context, u32* size) {
    if(context == NULL || size == NULL) {
        return R_FBI_INVALID_ARGUMENT;
    }

    return httpcGetDownloadSizeState(context, NULL, size);
}

Result util_http_get_file_name(httpcContext* context, char* out, u32 size) {
    if(context == NULL || out == NULL) {
        return R_FBI_INVALID_ARGUMENT;
    }

    Result res = 0;

    char* header = (char*) calloc(1, size + 64);
    if(header != NULL) {
        if(R_SUCCEEDED(res = httpcGetResponseHeader(context, "Content-Disposition", header, size + 64))) {
            char* start = strstr(header, "filename=");
            if(start != NULL) {
                char format[32];
                snprintf(format, sizeof(format), "filename=\"%%%lu[^\"]\"", size);
                if(sscanf(start, format, out) != 1) {
                    res = R_FBI_BAD_DATA;
                }
            } else {
                res = R_FBI_BAD_DATA;
            }
        }

        free(header);
    } else {
        res = R_FBI_OUT_OF_MEMORY;
    }

    return res;
}

Result util_http_read(httpcContext* context, u32* bytesRead, void* buffer, u32 size) {
    if(context == NULL || buffer == NULL) {
        return R_FBI_INVALID_ARGUMENT;
    }

    Result res = 0;

    u32 startPos = 0;
    if(R_SUCCEEDED(res = httpcGetDownloadSizeState(context, &startPos, NULL))) {
        res = HTTPC_RESULTCODE_DOWNLOADPENDING;

        u32 outPos = 0;
        while(res == HTTPC_RESULTCODE_DOWNLOADPENDING && outPos < size) {
            if(R_SUCCEEDED(res = httpcReceiveDataTimeout(context, &((u8*) buffer)[outPos], size - outPos, HTTPC_TIMEOUT)) || res == HTTPC_RESULTCODE_DOWNLOADPENDING) {
                Result posRes = 0;
                u32 currPos = 0;
                if(R_SUCCEEDED(posRes = httpcGetDownloadSizeState(context, &currPos, NULL))) {
                    outPos = currPos - startPos;
                } else {
                    res = posRes;
                }
            }
        }

        if(res == HTTPC_RESULTCODE_DOWNLOADPENDING) {
            res = 0;
        }

        if(R_SUCCEEDED(res) && bytesRead != NULL) {
            *bytesRead = outPos;
        }
    }

    return res;
}

Result util_http_close(httpcContext* context) {
    if(context == NULL) {
        return R_FBI_INVALID_ARGUMENT;
    }

    return httpcCloseContext(context);
}
