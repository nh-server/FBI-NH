#pragma once

#include <sys/syslimits.h>

#include "../list.h"

#define CIA_INSTALL_RESULT_CANCELLED -1
#define CIA_INSTALL_RESULT_ERRNO -2
#define CIA_INSTALL_RESULT_WRONG_SYSTEM -3

typedef struct {
    char shortDescription[0x81];
    char longDescription[0x161];
    char publisher[0x81];
    u32 texture;
} smdh_info;

typedef struct {
    u64 titleId;
    u16 version;
    u64 installedSizeSD;
    u64 installedSizeNAND;
    bool hasSmdh;
    smdh_info smdhInfo;
} cia_info;

typedef struct {
    FS_Archive* archive;
    char name[NAME_MAX];
    char path[PATH_MAX];
    bool isDirectory;
    bool containsCias;
    u64 size;
    bool isCia;
    cia_info ciaInfo;
} file_info;

typedef struct {
    FS_MediaType mediaType;
    u64 titleId;
    char productCode[0x10];
    u16 version;
    u64 installedSize;
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
    u64 systemSaveDataId;
} system_save_data_info;

void task_init();
void task_exit();
Handle task_get_mutex();

void task_refresh_files();
void task_refresh_titles();
void task_refresh_pending_titles();
void task_refresh_tickets();
void task_refresh_ext_save_data();
void task_refresh_system_save_data();

void task_request_cia_install(FS_MediaType dest, u64 size, void* data, Result (*read)(void* data, u32* bytesRead, void* buffer, u32 size));
Result task_get_cia_install_result();
int task_get_cia_install_errno();
bool task_is_cia_installing();
void task_cancel_cia_install();

FS_Archive* task_get_files_archive();
void task_set_files_archive(FS_Archive* archive);
char* task_get_files_path();
void task_set_files_path(char* path);
list_item* task_get_files();
u32* task_get_files_count();

list_item* task_get_titles();
u32* task_get_title_count();

list_item* task_get_pending_titles();
u32* task_get_pending_title_count();

list_item* task_get_tickets();
u32* task_get_ticket_count();

list_item* task_get_ext_save_data();
u32* task_get_ext_save_data_count();

list_item* task_get_system_save_data();
u32* task_get_system_save_data_count();

void ui_draw_ext_save_data_info(ui_view* view, void* data, float x1, float y1, float x2, float y2);
void ui_draw_file_info(ui_view* view, void* data, float x1, float y1, float x2, float y2);
void ui_draw_system_save_data_info(ui_view* view, void* data, float x1, float y1, float x2, float y2);
void ui_draw_title_info(ui_view* view, void* data, float x1, float y1, float x2, float y2);
void ui_draw_pending_title_info(ui_view* view, void* data, float x1, float y1, float x2, float y2);
void ui_draw_ticket_info(ui_view* view, void* data, float x1, float y1, float x2, float y2);