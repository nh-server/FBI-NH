#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <3ds.h>

#include "error.h"
#include "http.h"

#define HTTPC_TIMEOUT 15000000000

Result http_open(httpcContext* context, const char* url, bool userAgent) {
    return http_open_ranged(context, url, userAgent, 0, 0);
}

Result http_open_ranged(httpcContext* context, const char* url, bool userAgent, u32 rangeStart, u32 rangeEnd) {
    if(context == NULL || url == NULL) {
        return R_APP_INVALID_ARGUMENT;
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
                        res = R_APP_HTTP_ERROR_BASE + response;
                    }
                }
            }

            if(R_FAILED(res)) {
                httpcCloseContext(context);
            }
        }
    }

    if(R_SUCCEEDED(res) && redirectCount >= 32) {
        res = R_APP_HTTP_TOO_MANY_REDIRECTS;
    }

    return res;
}

Result http_get_size(httpcContext* context, u32* size) {
    if(context == NULL || size == NULL) {
        return R_APP_INVALID_ARGUMENT;
    }

    return httpcGetDownloadSizeState(context, NULL, size);
}

Result http_get_file_name(httpcContext* context, char* out, u32 size) {
    if(context == NULL || out == NULL) {
        return R_APP_INVALID_ARGUMENT;
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
                    res = R_APP_BAD_DATA;
                }
            } else {
                res = R_APP_BAD_DATA;
            }
        }

        free(header);
    } else {
        res = R_APP_OUT_OF_MEMORY;
    }

    return res;
}

Result http_read(httpcContext* context, u32* bytesRead, void* buffer, u32 size) {
    if(context == NULL || buffer == NULL) {
        return R_APP_INVALID_ARGUMENT;
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

Result http_close(httpcContext* context) {
    if(context == NULL) {
        return R_APP_INVALID_ARGUMENT;
    }

    return httpcCloseContext(context);
}