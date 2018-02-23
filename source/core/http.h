#pragma once

typedef struct http_context_s* http_context;

Result http_open(http_context* context, const char* url, bool userAgent);
Result http_open_ranged(http_context* context, const char* url, bool userAgent, u32 rangeStart, u32 rangeEnd);
Result http_close(http_context context);
Result http_get_size(http_context context, u32* size);
Result http_get_file_name(http_context context, char* out, u32 size);
Result http_read(http_context context, u32* bytesRead, void* buffer, u32 size);

Result http_download(const char* url, u32* downloadedSize, void* buf, size_t size);
Result http_download_json(const char* url, json_t** json, size_t maxSize);
Result http_download_seed(u64 titleId);