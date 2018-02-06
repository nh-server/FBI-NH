#pragma once

#define MAKE_HTTP_USER_AGENT_(major, minor, micro) ("Mozilla/5.0 (Nintendo 3DS; Mobile; rv:10.0) Gecko/20100101 FBI/" #major "." #minor "." #micro)
#define MAKE_HTTP_USER_AGENT(major, minor, micro) MAKE_HTTP_USER_AGENT_(major, minor, micro)
#define HTTP_USER_AGENT MAKE_HTTP_USER_AGENT(VERSION_MAJOR, VERSION_MINOR, VERSION_MICRO)

#define HTTP_CONNECT_TIMEOUT 15

Result http_open(httpcContext* context, const char* url, bool userAgent);
Result http_open_ranged(httpcContext* context, const char* url, bool userAgent, u32 rangeStart, u32 rangeEnd);
Result http_get_size(httpcContext* context, u32* size);
Result http_get_file_name(httpcContext* context, char* out, u32 size);
Result http_read(httpcContext* context, u32* bytesRead, void* buffer, u32 size);
Result http_close(httpcContext* context);