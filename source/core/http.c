#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <3ds.h>
#include <zlib.h>

#include "error.h"
#include "http.h"

#define HTTPC_TIMEOUT 15000000000

struct http_context_s {
    httpcContext httpc;

    bool compressed;
    z_stream inflate;
    u8 buffer[32 * 1024];
    u32 bufferSize;
};

Result http_open(http_context* context, const char* url, bool userAgent) {
    return http_open_ranged(context, url, userAgent, 0, 0);
}

Result http_open_ranged(http_context* context, const char* url, bool userAgent, u32 rangeStart, u32 rangeEnd) {
    if(url == NULL) {
        return R_APP_INVALID_ARGUMENT;
    }

    Result res = 0;

    http_context ctx = (http_context) calloc(1, sizeof(struct http_context_s));
    if(ctx != NULL) {
        char currUrl[1024];
        strncpy(currUrl, url, sizeof(currUrl));

        char range[64];
        if(rangeEnd > rangeStart) {
            snprintf(range, sizeof(range), "%lu-%lu", rangeStart, rangeEnd);
        } else {
            snprintf(range, sizeof(range), "%lu-", rangeStart);
        }

        bool resolved = false;
        u32 redirectCount = 0;
        while(R_SUCCEEDED(res) && !resolved && redirectCount < 32) {
            if(R_SUCCEEDED(res = httpcOpenContext(&ctx->httpc, HTTPC_METHOD_GET, currUrl, 1))) {
                u32 response = 0;
                if(R_SUCCEEDED(res = httpcSetSSLOpt(&ctx->httpc, SSLCOPT_DisableVerify))
                   && (!userAgent || R_SUCCEEDED(res = httpcAddRequestHeaderField(&ctx->httpc, "User-Agent", HTTP_USER_AGENT)))
                   && (rangeStart == 0 || R_SUCCEEDED(res = httpcAddRequestHeaderField(&ctx->httpc, "Range", range)))
                   && R_SUCCEEDED(res = httpcAddRequestHeaderField(&ctx->httpc, "Accept-Encoding", "gzip, deflate"))
                   && R_SUCCEEDED(res = httpcSetKeepAlive(&ctx->httpc, HTTPC_KEEPALIVE_ENABLED))
                   && R_SUCCEEDED(res = httpcBeginRequest(&ctx->httpc))
                   && R_SUCCEEDED(res = httpcGetResponseStatusCodeTimeout(&ctx->httpc, &response, HTTPC_TIMEOUT))) {
                    if(response == 301 || response == 302 || response == 303) {
                        redirectCount++;

                        memset(currUrl, '\0', sizeof(currUrl));
                        if(R_SUCCEEDED(res = httpcGetResponseHeader(&ctx->httpc, "Location", currUrl, sizeof(currUrl)))) {
                            httpcCloseContext(&ctx->httpc);
                        }
                    } else {
                        resolved = true;

                        if(response == 200) {
                            char encoding[32];
                            if(R_SUCCEEDED(httpcGetResponseHeader(&ctx->httpc, "Content-Encoding", encoding, sizeof(encoding)))) {
                                bool gzip = strncmp(encoding, "gzip", sizeof(encoding)) == 0;
                                bool deflate = strncmp(encoding, "deflate", sizeof(encoding)) == 0;

                                ctx->compressed = gzip || deflate;

                                if(ctx->compressed) {
                                    memset(&ctx->inflate, 0, sizeof(ctx->inflate));
                                    if(deflate) {
                                        inflateInit(&ctx->inflate);
                                    } else if(gzip) {
                                        inflateInit2(&ctx->inflate, MAX_WBITS | 16);
                                    }
                                }
                            }
                        } else {
                            res = R_APP_HTTP_ERROR_BASE + response;
                        }
                    }
                }

                if(R_FAILED(res)) {
                    httpcCloseContext(&ctx->httpc);
                }
            }
        }

        if(R_SUCCEEDED(res) && redirectCount >= 32) {
            res = R_APP_HTTP_TOO_MANY_REDIRECTS;
        }

        if(R_FAILED(res)) {
            free(ctx);
        }
    } else {
        res = R_APP_OUT_OF_MEMORY;
    }

    if(R_SUCCEEDED(res)) {
        *context = ctx;
    }

    return res;
}

Result http_close(http_context context) {
    if(context == NULL) {
        return R_APP_INVALID_ARGUMENT;
    }

    if(context->compressed) {
        inflateEnd(&context->inflate);
    }

    Result res = httpcCloseContext(&context->httpc);
    free(context);
    return res;
}

Result http_get_size(http_context context, u32* size) {
    if(context == NULL || size == NULL) {
        return R_APP_INVALID_ARGUMENT;
    }

    return httpcGetDownloadSizeState(&context->httpc, NULL, size);
}

Result http_get_file_name(http_context context, char* out, u32 size) {
    if(context == NULL || out == NULL) {
        return R_APP_INVALID_ARGUMENT;
    }

    Result res = 0;

    char* header = (char*) calloc(1, size + 64);
    if(header != NULL) {
        if(R_SUCCEEDED(res = httpcGetResponseHeader(&context->httpc, "Content-Disposition", header, size + 64))) {
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

Result http_read(http_context context, u32* bytesRead, void* buffer, u32 size) {
    if(context == NULL || buffer == NULL) {
        return R_APP_INVALID_ARGUMENT;
    }

    Result res = 0;

    u32 startPos = 0;
    if(R_SUCCEEDED(res = httpcGetDownloadSizeState(&context->httpc, &startPos, NULL))) {
        res = HTTPC_RESULTCODE_DOWNLOADPENDING;

        u32 outPos = 0;
        if(context->compressed) {
            u32 lastPos = context->bufferSize;
            while(res == HTTPC_RESULTCODE_DOWNLOADPENDING && outPos < size) {
                if((context->bufferSize > 0
                    || R_SUCCEEDED(res = httpcReceiveDataTimeout(&context->httpc, &context->buffer[context->bufferSize], sizeof(context->buffer) - context->bufferSize, HTTPC_TIMEOUT))
                    || res == HTTPC_RESULTCODE_DOWNLOADPENDING)) {
                    Result posRes = 0;
                    u32 currPos = 0;
                    if(R_SUCCEEDED(posRes = httpcGetDownloadSizeState(&context->httpc, &currPos, NULL))) {
                        context->bufferSize += currPos - lastPos;

                        context->inflate.next_in = context->buffer;
                        context->inflate.next_out = buffer + outPos;
                        context->inflate.avail_in = context->bufferSize;
                        context->inflate.avail_out = size - outPos;
                        inflate(&context->inflate, Z_SYNC_FLUSH);

                        memcpy(context->buffer, context->buffer + (context->bufferSize - context->inflate.avail_in), context->inflate.avail_in);
                        context->bufferSize = context->inflate.avail_in;

                        lastPos = currPos;
                        outPos = size - context->inflate.avail_out;
                    } else {
                        res = posRes;
                    }
                }
            }
        } else {
            while(res == HTTPC_RESULTCODE_DOWNLOADPENDING && outPos < size) {
                if(R_SUCCEEDED(res = httpcReceiveDataTimeout(&context->httpc, &((u8*) buffer)[outPos], size - outPos, HTTPC_TIMEOUT)) || res == HTTPC_RESULTCODE_DOWNLOADPENDING) {
                    Result posRes = 0;
                    u32 currPos = 0;
                    if(R_SUCCEEDED(posRes = httpcGetDownloadSizeState(&context->httpc, &currPos, NULL))) {
                        outPos = currPos - startPos;
                    } else {
                        res = posRes;
                    }
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