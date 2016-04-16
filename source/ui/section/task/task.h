#pragma once

#include <sys/syslimits.h>

#include "../../list.h"

typedef struct {
    char shortDescription[0x100];
    char longDescription[0x200];
    char publisher[0x100];
    u32 texture;
} smdh_info;

typedef struct {
    FS_MediaType mediaType;
    u64 titleId;
    char productCode[0x10];
    u16 version;
    u64 installedSize;
    bool twl;
    bool hasSmdh;
    smdh_info smdhInfo;
} title_info;

typedef struct {
    FS_MediaType mediaType;
    u64 titleId;
    u16 version;
} pending_title_info;

typedef struct {
    u64 ticketId;
} ticket_info;

typedef struct {
    FS_MediaType mediaType;
    u64 extSaveDataId;
    bool shared;
    bool hasSmdh;
    smdh_info smdhInfo;
} ext_save_data_info;

typedef struct {
    u32 systemSaveDataId;
} system_save_data_info;

typedef struct {
    u64 titleId;
    u16 version;
    u64 installedSize;
    bool hasSmdh;
    smdh_info smdhInfo;
} cia_info;

typedef struct {
    FS_Archive* archive;
    char name[NAME_MAX];
    char path[PATH_MAX];
    bool isDirectory;
    u64 size;

    bool containsCias;
    bool isCia;
    cia_info ciaInfo;

    bool containsTickets;
    bool isTicket;
    ticket_info ticketInfo;
} file_info;

typedef struct {
    bool finished;
    bool failed;
    bool cancelled;
    bool ioerr;
    bool wrongSystem;

    Result result;
    int ioerrno;
} install_cia_result;

bool task_is_quit_all();
void task_quit_all();

Handle task_populate_ext_save_data(list_item* items, u32* count, u32 max);
Handle task_populate_files(list_item* items, u32* count, u32 max, file_info* dir);
Handle task_populate_pending_titles(list_item* items, u32* count, u32 max);
Handle task_populate_system_save_data(list_item* items, u32* count, u32 max);
Handle task_populate_tickets(list_item* items, u32* count, u32 max);
Handle task_populate_titles(list_item* items, u32* count, u32 max);
Handle task_install_cia(install_cia_result* result, u64 size, void* data, Result (*read)(void* data, u32* bytesRead, void* buffer, u32 size));