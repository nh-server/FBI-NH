#pragma once

typedef struct json_t json_t;

// Errors
#define R_FBI_CANCELLED MAKERESULT(RL_PERMANENT, RS_CANCELED, RM_APPLICATION, 1)
#define R_FBI_HTTP_RESPONSE_CODE MAKERESULT(RL_PERMANENT, RS_INTERNAL, RM_APPLICATION, 2)
#define R_FBI_WRONG_SYSTEM MAKERESULT(RL_PERMANENT, RS_NOTSUPPORTED, RM_APPLICATION, 3)
#define R_FBI_INVALID_ARGUMENT MAKERESULT(RL_PERMANENT, RS_INVALIDARG, RM_APPLICATION, 4)
#define R_FBI_THREAD_CREATE_FAILED MAKERESULT(RL_PERMANENT, RS_INTERNAL, RM_APPLICATION, 5)
#define R_FBI_PARSE_FAILED MAKERESULT(RL_PERMANENT, RS_INTERNAL, RM_APPLICATION, 6)
#define R_FBI_BAD_DATA MAKERESULT(RL_PERMANENT, RS_INTERNAL, RM_APPLICATION, 7)
#define R_FBI_TOO_MANY_REDIRECTS MAKERESULT(RL_PERMANENT, RS_INTERNAL, RM_APPLICATION, 8)

#define R_FBI_CURL_INIT_FAILED MAKERESULT(RL_PERMANENT, RS_INTERNAL, RM_APPLICATION, 9)
#define R_FBI_CURL_ERORR_BASE MAKERESULT(RL_PERMANENT, RS_INTERNAL, RM_APPLICATION, 10)

#define R_FBI_NOT_IMPLEMENTED MAKERESULT(RL_PERMANENT, RS_INTERNAL, RM_APPLICATION, RD_NOT_IMPLEMENTED)
#define R_FBI_OUT_OF_MEMORY MAKERESULT(RL_FATAL, RS_OUTOFRESOURCE, RM_APPLICATION, RD_OUT_OF_MEMORY)
#define R_FBI_OUT_OF_RANGE MAKERESULT(RL_PERMANENT, RS_INVALIDARG, RM_APPLICATION, RD_OUT_OF_RANGE)

// HTTP constants
#define MAKE_HTTP_USER_AGENT_(major, minor, micro) ("Mozilla/5.0 (Nintendo 3DS; Mobile; rv:10.0) Gecko/20100101 FBI/" #major "." #minor "." #micro)
#define MAKE_HTTP_USER_AGENT(major, minor, micro) MAKE_HTTP_USER_AGENT_(major, minor, micro)
#define HTTP_USER_AGENT MAKE_HTTP_USER_AGENT(VERSION_MAJOR, VERSION_MINOR, VERSION_MICRO)

#define HTTP_CONNECT_TIMEOUT 15

// File constants
#define FILE_NAME_MAX 512
#define FILE_PATH_MAX 512

// Panic
void util_panic(const char* s, ...);

// Strings
bool util_is_string_empty(const char* str);

// Files
Result util_open_archive(FS_Archive* archive, FS_ArchiveID id, FS_Path path);
Result util_ref_archive(FS_Archive archive);
Result util_close_archive(FS_Archive archive);

const char* util_get_3dsx_path();
void util_set_3dsx_path(const char* path);

FS_Path util_make_binary_path(const void* data, u32 size);
FS_Path* util_make_path_utf8(const char* path);
void util_free_path_utf8(FS_Path* path);

bool util_is_dir(FS_Archive archive, const char* path);
Result util_ensure_dir(FS_Archive archive, const char* path);

void util_get_file_name(char* out, const char* file, u32 size);
void util_escape_file_name(char* out, const char* file, size_t size);
void util_get_path_file(char* out, const char* path, u32 size);
void util_get_parent_path(char* out, const char* path, u32 size);

bool util_filter_cias(void* data, const char* name, u32 attributes);
bool util_filter_tickets(void* data, const char* name, u32 attributes);

// Titles
Result util_import_seed(u32* responseCode, u64 titleId);

FS_MediaType util_get_title_destination(u64 titleId);

// Download
Result util_download(const char* url, u32* downloadedSize, void* buf, size_t size);
Result util_download_json(const char* url, json_t** json, size_t maxSize);

// HTTP
Result util_http_open(httpcContext* context, u32* responseCode, const char* url, bool userAgent);
Result util_http_open_ranged(httpcContext* context, u32* responseCode, const char* url, bool userAgent, u32 rangeStart, u32 rangeEnd);
Result util_http_get_size(httpcContext* context, u32* size);
Result util_http_get_file_name(httpcContext* context, char* out, u32 size);
Result util_http_read(httpcContext* context, u32* bytesRead, void* buffer, u32 size);
Result util_http_close(httpcContext* context);