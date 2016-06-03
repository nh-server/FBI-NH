#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <3ds.h>

#include "util.h"
#include "../ui/error.h"
#include "../ui/list.h"
#include "../ui/section/task/task.h"
#include "linkedlist.h"

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

static Result FSUSER_AddSeed(u64 titleId, const void* seed) {
    u32 *cmdbuf = getThreadCommandBuffer();

    cmdbuf[0] = 0x087a0180;
    cmdbuf[1] = (u32) (titleId & 0xFFFFFFFF);
    cmdbuf[2] = (u32) (titleId >> 32);
    memcpy(&cmdbuf[3], seed, 16);

    Result ret = 0;
    if(R_FAILED(ret = svcSendSyncRequest(*fsGetSessionHandle()))) return ret;

    ret = cmdbuf[1];
    return ret;
}

Result util_import_seed(u64 titleId) {
    char pathBuf[64];
    snprintf(pathBuf, 64, "/fbi/seed/%016llX.dat", titleId);

    Result res = 0;

    FS_Path* fsPath = util_make_path_utf8(pathBuf);
    if(fsPath != NULL) {
        u8 seed[16];

        Handle fileHandle = 0;
        if(R_SUCCEEDED(res = FSUSER_OpenFileDirectly(&fileHandle, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""), *fsPath, FS_OPEN_READ, 0))) {
            u32 bytesRead = 0;
            res = FSFILE_Read(fileHandle, &bytesRead, 0, seed, sizeof(seed));

            FSFILE_Close(fileHandle);
        }

        util_free_path_utf8(fsPath);

        if(R_FAILED(res)) {
            static const char* regionStrings[] = {"JP", "US", "GB", "GB", "HK", "KR", "TW"};

            u8 region = CFG_REGION_USA;
            CFGU_SecureInfoGetRegion(&region);

            if(region <= CFG_REGION_TWN) {
                char url[128];
                snprintf(url, 128, "https://kagiya-ctr.cdn.nintendo.net/title/0x%016llX/ext_key?country=%s", titleId, regionStrings[region]);

                httpcContext context;
                if(R_SUCCEEDED(res = httpcOpenContext(&context, HTTPC_METHOD_GET, url, 1))) {
                    u32 responseCode = 0;
                    if(R_SUCCEEDED(res = httpcSetSSLOpt(&context, SSLCOPT_DisableVerify)) && R_SUCCEEDED(res = httpcBeginRequest(&context)) && R_SUCCEEDED(res = httpcGetResponseStatusCode(&context, &responseCode, 0))) {
                        if(responseCode == 200) {
                            u32 bytesRead = 0;
                            res = httpcDownloadData(&context, seed, sizeof(seed), &bytesRead);
                        } else {
                            res = R_FBI_HTTP_RESPONSE_CODE;
                        }
                    }

                    httpcCloseContext(&context);
                }
            } else {
                res = R_FBI_OUT_OF_RANGE;
            }
        }

        if(R_SUCCEEDED(res)) {
            res = FSUSER_AddSeed(titleId, seed);
        }
    } else {
        res = R_FBI_OUT_OF_MEMORY;
    }

    return res;
}

static u32 sigSizes[6] = {0x240, 0x140, 0x80, 0x240, 0x140, 0x80};

u64 util_get_cia_title_id(u8* cia) {
    u32 headerSize = *(u32*) &cia[0x00];
    u32 certSize = *(u32*) &cia[0x08];
    u32 ticketSize = *(u32*) &cia[0x0C];

    u8* tmd = &cia[((headerSize + 0x3F) & ~0x3F) + ((certSize + 0x3F) & ~0x3F) + ((ticketSize + 0x3F) & ~0x3F)];

    return util_get_tmd_title_id(tmd);
}

u64 util_get_ticket_title_id(u8* ticket) {
    return __builtin_bswap64(*(u64*) &ticket[sigSizes[ticket[0x03]] + 0x9C]);
}

u64 util_get_tmd_title_id(u8* tmd) {
    return __builtin_bswap64(*(u64*) &tmd[sigSizes[tmd[0x03]] + 0x4C]);
}

u16 util_get_tmd_content_count(u8* tmd) {
    return __builtin_bswap16(*(u16*) &tmd[sigSizes[tmd[0x03]] + 0x9E]);
}

u8* util_get_tmd_content_chunk(u8* tmd, u32 index) {
    return &tmd[sigSizes[tmd[0x03]] + 0x9C4 + (index * 0x30)];
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
    return len >= 4 && strncasecmp(name + len - 4, ".tik", 4) == 0;
}

int util_compare_file_infos(const void** p1, const void** p2) {
    list_item* info1 = *(list_item**) p1;
    list_item* info2 = *(list_item**) p2;

    bool info1Base = strncmp(info1->name, "<current directory>", LIST_ITEM_NAME_MAX) == 0 || strncmp(info1->name, "<current file>", LIST_ITEM_NAME_MAX) == 0;
    bool info2Base = strncmp(info2->name, "<current directory>", LIST_ITEM_NAME_MAX) == 0 || strncmp(info2->name, "<current file>", LIST_ITEM_NAME_MAX) == 0;

    if(info1Base && !info2Base) {
        return -1;
    } else if(!info1Base && info2Base) {
        return 1;
    } else {
        file_info* f1 = (file_info*) info1->data;
        file_info* f2 = (file_info*) info2->data;

        if(f1->isDirectory && !f2->isDirectory) {
            return -1;
        } else if(!f1->isDirectory && f2->isDirectory) {
            return 1;
        } else {
            return strncasecmp(f1->name, f2->name, FILE_NAME_MAX);
        }
    }
}

static const char* path_3dsx = NULL;

const char* util_get_3dsx_path() {
    return path_3dsx;
}

void util_set_3dsx_path(const char* path) {
    path_3dsx = path;
}

typedef struct {
    FS_Archive archive;
    u32 refs;
} archive_ref;

static linked_list opened_archives;

static Result util_add_archive_ref(FS_Archive archive) {
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

Result util_open_archive(FS_Archive* archive, FS_ArchiveID id, FS_Path path) {
    if(archive == NULL) {
        return R_FBI_INVALID_ARGUMENT;
    }

    Result res = 0;

    FS_Archive arch = 0;
    if(R_SUCCEEDED(res = FSUSER_OpenArchive(&arch, id, path))) {
        if(R_SUCCEEDED(res = util_add_archive_ref(arch))) {
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

    return util_add_archive_ref(archive);
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
