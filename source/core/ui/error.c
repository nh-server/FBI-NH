#include <malloc.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <3ds.h>
#include <curl/curl.h>

#include "error.h"
#include "prompt.h"
#include "../error.h"
#include "../screen.h"
#include "../../fbi/resources.h"

static const char* level_to_string(Result res) {
    switch(R_LEVEL(res)) {
        case RL_SUCCESS:
            return "Success";
        case RL_INFO:
            return "Info";
        case RL_FATAL:
            return "Fatal";
        case RL_RESET:
            return "Reset";
        case RL_REINITIALIZE:
            return "Reinitialize";
        case RL_USAGE:
            return "Usage";
        case RL_PERMANENT:
            return "Permanent";
        case RL_TEMPORARY:
            return "Temporary";
        case RL_STATUS:
            return "Status";
        default:
            return "<unknown>";
    }
}

static const char* summary_to_string(Result res) {
    switch(R_SUMMARY(res)) {
        case RS_SUCCESS:
            return "Success";
        case RS_NOP:
            return "Nop";
        case RS_WOULDBLOCK:
            return "Would block";
        case RS_OUTOFRESOURCE:
            return "Out of resource";
        case RS_NOTFOUND:
            return "Not found";
        case RS_INVALIDSTATE:
            return "Invalid state";
        case RS_NOTSUPPORTED:
            return "Not supported";
        case RS_INVALIDARG:
            return "Invalid argument";
        case RS_WRONGARG:
            return "Wrong argument";
        case RS_CANCELED:
            return "Canceled";
        case RS_STATUSCHANGED:
            return "Status changed";
        case RS_INTERNAL:
            return "Internal";
        default:
            return "<unknown>";
    }
}

static const char* module_to_string(Result res) {
    switch(R_MODULE(res)) {
        case RM_COMMON:
            return "Common";
        case RM_KERNEL:
            return "Kernel";
        case RM_UTIL:
            return "Util";
        case RM_FILE_SERVER:
            return "File server";
        case RM_LOADER_SERVER:
            return "Loader server";
        case RM_TCB:
            return "TCB";
        case RM_OS:
            return "OS";
        case RM_DBG:
            return "DBG";
        case RM_DMNT:
            return "DMNT";
        case RM_PDN:
            return "PDN";
        case RM_GSP:
            return "GSP";
        case RM_I2C:
            return "I2C";
        case RM_GPIO:
            return "GPIO";
        case RM_DD:
            return "DD";
        case RM_CODEC:
            return "CODEC";
        case RM_SPI:
            return "SPI";
        case RM_PXI:
            return "PXI";
        case RM_FS:
            return "FS";
        case RM_DI:
            return "DI";
        case RM_HID:
            return "HID";
        case RM_CAM:
            return "CAM";
        case RM_PI:
            return "PI";
        case RM_PM:
            return "PM";
        case RM_PM_LOW:
            return "PMLOW";
        case RM_FSI:
            return "FSI";
        case RM_SRV:
            return "SRV";
        case RM_NDM:
            return "NDM";
        case RM_NWM:
            return "NWM";
        case RM_SOC:
            return "SOC";
        case RM_LDR:
            return "LDR";
        case RM_ACC:
            return "ACC";
        case RM_ROMFS:
            return "RomFS";
        case RM_AM:
            return "AM";
        case RM_HIO:
            return "HIO";
        case RM_UPDATER:
            return "Updater";
        case RM_MIC:
            return "MIC";
        case RM_FND:
            return "FND";
        case RM_MP:
            return "MP";
        case RM_MPWL:
            return "MPWL";
        case RM_AC:
            return "AC";
        case RM_HTTP:
            return "HTTP";
        case RM_DSP:
            return "DSP";
        case RM_SND:
            return "SND";
        case RM_DLP:
            return "DLP";
        case RM_HIO_LOW:
            return "HIOLOW";
        case RM_CSND:
            return "CSND";
        case RM_SSL:
            return "SSL";
        case RM_AM_LOW:
            return "AMLOW";
        case RM_NEX:
            return "NEX";
        case RM_FRIENDS:
            return "Friends";
        case RM_RDT:
            return "RDT";
        case RM_APPLET:
            return "Applet";
        case RM_NIM:
            return "NIM";
        case RM_PTM:
            return "PTM";
        case RM_MIDI:
            return "MIDI";
        case RM_MC:
            return "MC";
        case RM_SWC:
            return "SWC";
        case RM_FATFS:
            return "FatFS";
        case RM_NGC:
            return "NGC";
        case RM_CARD:
            return "CARD";
        case RM_CARDNOR:
            return "CARDNOR";
        case RM_SDMC:
            return "SDMC";
        case RM_BOSS:
            return "BOSS";
        case RM_DBM:
            return "DBM";
        case RM_CONFIG:
            return "Config";
        case RM_PS:
            return "PS";
        case RM_CEC:
            return "CEC";
        case RM_IR:
            return "IR";
        case RM_UDS:
            return "UDS";
        case RM_PL:
            return "PL";
        case RM_CUP:
            return "CUP";
        case RM_GYROSCOPE:
            return "Gyroscope";
        case RM_MCU:
            return "MCU";
        case RM_NS:
            return "NS";
        case RM_NEWS:
            return "NEWS";
        case RM_RO:
            return "RO";
        case RM_GD:
            return "GD";
        case RM_CARD_SPI:
            return "CARDSPI";
        case RM_EC:
            return "EC";
        case RM_WEB_BROWSER:
            return "Web browser";
        case RM_TEST:
            return "TEST";
        case RM_ENC:
            return "ENC";
        case RM_PIA:
            return "PIA";
        case RM_ACT:
            return "ACT";
        case RM_VCTL:
            return "VCTL";
        case RM_OLV:
            return "OLV";
        case RM_NEIA:
            return "NEIA";
        case RM_NPNS:
            return "NPNS";
        case RM_AVD:
            return "AVD";
        case RM_L2B:
            return "L2B";
        case RM_MVD:
            return "MVD";
        case RM_NFC:
            return "NFC";
        case RM_UART:
            return "UART";
        case RM_SPM:
            return "SPM";
        case RM_QTM:
            return "QTM";
        case RM_NFP:
            return "NFP";
        case RM_APPLICATION:
            return "Application";
        default:
            return "<unknown>";
    }
}

static const char* description_to_string(Result res) {
    int module = R_MODULE(res);
    int description = R_DESCRIPTION(res);

    switch(module) {
        case RM_KERNEL:
            switch(description) {
                case 2:
                    return "Invalid DMA buffer memory permissions";
                default:
                    break;
            }

            break;
        case RM_OS:
            switch(description) {
                case 1:
                    return "Out of synchronization object";
                case 2:
                    return "Out of shared memory objects";
                case 9:
                    return "Out of session objects";
                case 10:
                    return "Not enough memory for allocation";
                case 20:
                    return "Wrong permissions for unprivileged access";
                case 26:
                    return "Session closed by remote process";
                case 47:
                    return "Invalid command header";
                case 52:
                    return "Max port connections exceeded";
                default:
                    break;
            }

            break;
        case RM_FS:
            switch(description) {
                case 101:
                    return "Archive not mounted";
                case 120:
                    return "Doesn't exist / Failed to open";
                case 141:
                    return "Game card not inserted";
                case 171:
                    return "Bus: Busy / Underrun";
                case 172:
                    return "Bus: Illegal function";
                case 190:
                    return "Already exists / Failed to create";
                case 210:
                    return "Partition full";
                case 230:
                    return "Illegal operation / File in use";
                case 231:
                    return "Resource locked";
                case 250:
                    return "FAT operation denied";
                case 265:
                    return "Bus: Timeout";
                case 331:
                    return "Bus error / TWL partition invalid";
                case 332:
                    return "Bus: Stop bit error";
                case 391:
                    return "Hash verification failure";
                case 392:
                    return "RSA/Hash verification failure";
                case 395:
                    return "Invalid RomFS or save data block hash";
                case 630:
                    return "Archive permission denied";
                case 702:
                    return "Invalid path / Inaccessible archive";
                case 705:
                    return "Offset out of bounds";
                case 721:
                    return "Reached file size limit";
                case 760:
                    return "Unsupported operation";
                case 761:
                    return "ExeFS read size mismatch";
                default:
                    break;
            }

            break;
        case RM_SRV:
            switch(description) {
                case 5:
                    return "Invalid service name length";
                case 6:
                    return "Service access denied";
                case 7:
                    return "String size mismatch";
                default:
                    break;
            }

            break;
        case RM_AM:
            switch(description) {
                case 4:
                    return "Wrong installation state";
                case 37:
                    return "Invalid NCCH";
                case 39:
                    return "Invalid or outdated title version";
                case 41:
                    return "Error type 1";
                case 43:
                    return "Database does not exist";
                case 44:
                    return "Attempted to delete system title";
                case 101:
                    return "Error type -1";
                case 102:
                    return "Error type -2";
                case 103:
                    return "Error type -3";
                case 104:
                    return "Error type -4";
                case 105:
                    return "Error type -5";
                case 106:
                    return "Cert signature or hash check failed";
                case 107:
                    return "Error type -7";
                case 108:
                    return "Error type -8";
                case 109:
                    return "Error type -9";
                case 110:
                    return "Error type -10";
                case 111:
                    return "Error type -11";
                case 112:
                    return "Error type -12";
                case 113:
                    return "Error type -13";
                case 114:
                    return "Error type -14";
                case 393:
                    return "Invalid database";
                default:
                    break;
            }

            break;
        case RM_HTTP:
            switch(description) {
                case 60:
                    return "Failed to verify TLS certificate";
                case 70:
                    return "Network unavailable";
                case 102:
                    return "Wrong context handle";
                case 105:
                    return "Request timed out";
                default:
                    break;
            }

            break;
        case RM_SSL:
            switch(description) {
                case 20:
                    return "Untrusted RootCA";
                case 54:
                    return "RootCertChain handle not found";
                default:
                    break;
            }

            break;
        case RM_SDMC:
            switch(description) {
                case 1:
                    return "Bus: Bit23 error";
                case 2:
                    return "Bus: RX ready error";
                case 3:
                    return "Bus: Bit28 error";
                case 4:
                    return "Bus: Bit27 error";
                default:
                    break;
            }

            break;
        case RM_MVD:
            switch(description) {
                case 271:
                    return "Invalid configuration";
                default:
                    break;
            }

            break;
        case RM_NFC:
            switch(description) {
                case 512:
                    return "Invalid NFC state";
                default:
                    break;
            }

            break;
        case RM_QTM:
            switch(description) {
                case 8:
                    return "Camera busy";
                default:
                    break;
            }

            break;
        case RM_APPLICATION:
            switch(res) {
                case R_APP_INVALID_ARGUMENT:
                    return "Invalid argument";
                case R_APP_CANCELLED:
                    return "Operation cancelled";
                case R_APP_SKIPPED:
                    return "Operation skipped";
                case R_APP_THREAD_CREATE_FAILED:
                    return "Thread creation failed";
                case R_APP_PARSE_FAILED:
                    return "Parse failed";
                case R_APP_BAD_DATA:
                    return "Bad data";
                case R_APP_HTTP_TOO_MANY_REDIRECTS:
                    return "Too many redirects";
                default:
                    if(res >= R_APP_HTTP_ERROR_BASE && res < R_APP_HTTP_ERROR_END) {
                        switch(res - R_APP_HTTP_ERROR_BASE) {
                            case 100:
                                return "HTTP 100: Continue";
                            case 101:
                                return "HTTP 101: Switching Protocols";
                            case 102:
                                return "HTTP 102: Processing";
                            case 103:
                                return "HTTP 103: Early Hints";
                            case 200:
                                return "HTTP 200: OK";
                            case 201:
                                return "HTTP 201: Created";
                            case 202:
                                return "HTTP 202: Accepted";
                            case 203:
                                return "HTTP 203: Non-Authoritative Information";
                            case 204:
                                return "HTTP 204: No Content";
                            case 205:
                                return "HTTP 205: Reset Content";
                            case 206:
                                return "HTTP 206: Partial Content";
                            case 207:
                                return "HTTP 207: Multi-Status";
                            case 208:
                                return "HTTP 208: Already Reported";
                            case 226:
                                return "HTTP 226: IM Used";
                            case 300:
                                return "HTTP 300: Multiple Choices";
                            case 301:
                                return "HTTP 301: Moved Permanently";
                            case 302:
                                return "HTTP 302: Found";
                            case 303:
                                return "HTTP 303: See Other";
                            case 304:
                                return "HTTP 304: Not Modified";
                            case 305:
                                return "HTTP 305: Use Proxy";
                            case 306:
                                return "HTTP 306: Switch Proxy";
                            case 307:
                                return "HTTP 307: Temporary Redirect";
                            case 308:
                                return "HTTP 308: Permanent Redirect";
                            case 400:
                                return "HTTP 400: Bad Request";
                            case 401:
                                return "HTTP 401: Unauthorized";
                            case 402:
                                return "HTTP 402: Payment Required";
                            case 403:
                                return "HTTP 403: Forbidden";
                            case 404:
                                return "HTTP 404: Not Found";
                            case 405:
                                return "HTTP 405: Method Not Allowed";
                            case 406:
                                return "HTTP 406: Not Acceptable";
                            case 407:
                                return "HTTP 407: Proxy Authentication Required";
                            case 408:
                                return "HTTP 408: Request Timeout";
                            case 409:
                                return "HTTP 409: Conflict";
                            case 410:
                                return "HTTP 410: Gone";
                            case 411:
                                return "HTTP 411: Length Required";
                            case 412:
                                return "HTTP 412: Precondition Failed";
                            case 413:
                                return "HTTP 413: Payload Too Large";
                            case 414:
                                return "HTTP 414: URI Too Long";
                            case 415:
                                return "HTTP 415: Unsupported Media Type";
                            case 416:
                                return "HTTP 416: Range Not Satisfiable";
                            case 417:
                                return "HTTP 417: Expectation Failed";
                            case 418:
                                return "HTTP 418: I'm a teapot";
                            case 421:
                                return "HTTP 421: Misdirected Request";
                            case 422:
                                return "HTTP 422: Unprocessable Entity";
                            case 423:
                                return "HTTP 423: Locked";
                            case 424:
                                return "HTTP 424: Failed Dependency";
                            case 426:
                                return "HTTP 426: Upgrade Required";
                            case 428:
                                return "HTTP 428: Precondition Required";
                            case 429:
                                return "HTTP 429: Too Many Requests";
                            case 431:
                                return "HTTP 431: Request Header Fields Too Large";
                            case 451:
                                return "HTTP 451: Unavailable For Legal Reasons";
                            case 500:
                                return "HTTP 500: Internal Server Error";
                            case 501:
                                return "HTTP 501: Not Implemented";
                            case 502:
                                return "HTTP 502: Bad Gateway";
                            case 503:
                                return "HTTP 503: Service Unavailable";
                            case 504:
                                return "HTTP 504: Gateway Timeout";
                            case 505:
                                return "HTTP 505: HTTP Version Not Specified";
                            case 506:
                                return "HTTP 506: Variant Also Negotiates";
                            case 507:
                                return "HTTP 507: Insufficient Storage";
                            case 508:
                                return "HTTP 508: Loop Detected";
                            case 510:
                                return "HTTP 510: Not Extended";
                            case 511:
                                return "HTTP 511: Network Authentication Required";
                            default:
                                return "HTTP: Unknown Response Code";
                        }
                    } else if(res >= R_APP_CURL_ERROR_BASE && res < R_APP_CURL_ERROR_END) {
                        return curl_easy_strerror(res - R_APP_CURL_ERROR_BASE);
                    }

                    break;
            }
        default:
            break;
    }

    switch(description) {
        case RD_SUCCESS:
            return "Success";
        case RD_TIMEOUT:
            return "Timeout";
        case RD_OUT_OF_RANGE:
            return "Out of range";
        case RD_ALREADY_EXISTS:
            return "Already exists";
        case RD_CANCEL_REQUESTED:
            return "Cancel requested";
        case RD_NOT_FOUND:
            return "Not found";
        case RD_ALREADY_INITIALIZED:
            return "Already initialized";
        case RD_NOT_INITIALIZED:
            return "Not initialized";
        case RD_INVALID_HANDLE:
            return "Invalid handle";
        case RD_INVALID_POINTER:
            return "Invalid pointer";
        case RD_INVALID_ADDRESS:
            return "Invalid address";
        case RD_NOT_IMPLEMENTED:
            return "Not implemented";
        case RD_OUT_OF_MEMORY:
            return "Out of memory";
        case RD_MISALIGNED_SIZE:
            return "Misaligned size";
        case RD_MISALIGNED_ADDRESS:
            return "Misaligned address";
        case RD_BUSY:
            return "Busy";
        case RD_NO_DATA:
            return "No data";
        case RD_INVALID_COMBINATION:
            return "Invalid combination";
        case RD_INVALID_ENUM_VALUE:
            return "Invalid enum value";
        case RD_INVALID_SIZE:
            return "Invalid size";
        case RD_ALREADY_DONE:
            return "Already done";
        case RD_NOT_AUTHORIZED:
            return "Not authorized";
        case RD_TOO_LARGE:
            return "Too large";
        case RD_INVALID_SELECTION:
            return "Invalid selection";
        default:
            return "<unknown>";
    }
}

typedef struct {
    char fullText[4096];
    void* data;
    void (*drawTop)(ui_view* view, void* data, float x1, float y1, float x2, float y2);
} error_data;

static void error_draw_top(ui_view* view, void* data, float x1, float y1, float x2, float y2) {
    error_data* errorData = (error_data*) data;

    if(errorData->drawTop != NULL) {
        errorData->drawTop(view, errorData->data, x1, y1, x2, y2);
    }
}

static void error_onresponse(ui_view* view, void* data, u32 response) {
    free(data);
}

ui_view* error_display(void* data, void (*drawTop)(ui_view* view, void* data, float x1, float y1, float x2, float y2), const char* text, ...) {
    error_data* errorData = (error_data*) calloc(1, sizeof(error_data));
    if(errorData == NULL) {
        // No use trying to spawn another if we're out of memory.
        return NULL;
    }

    errorData->data = data;
    errorData->drawTop = drawTop;

    va_list list;
    va_start(list, text);
    vsnprintf(errorData->fullText, 4096, text, list);
    va_end(list);

    return prompt_display_notify("Error", errorData->fullText, COLOR_TEXT, errorData, error_draw_top, error_onresponse);
}

ui_view* error_display_res(void* data, void (*drawTop)(ui_view* view, void* data, float x1, float y1, float x2, float y2), Result result, const char* text, ...) {
    error_data* errorData = (error_data*) calloc(1, sizeof(error_data));
    if(errorData == NULL) {
        // No use trying to spawn another if we're out of memory.
        return NULL;
    }

    errorData->data = data;
    errorData->drawTop = drawTop;

    char textBuf[1024];
    va_list list;
    va_start(list, text);
    vsnprintf(textBuf, 1024, text, list);
    va_end(list);

    int level = R_LEVEL(result);
    int summary = R_SUMMARY(result);
    int module = R_MODULE(result);
    int description = R_DESCRIPTION(result);

    snprintf(errorData->fullText, 4096, "%s\nResult code: 0x%08lX\nLevel: %s (%d)\nSummary: %s (%d)\nModule: %s (%d)\nDesc: %s (%d)", textBuf, result, level_to_string(result), level, summary_to_string(result), summary, module_to_string(result), module, description_to_string(result), description);

    return prompt_display_notify("Error", errorData->fullText, COLOR_TEXT, errorData, error_draw_top, error_onresponse);
}

ui_view* error_display_errno(void* data, void (*drawTop)(ui_view* view, void* data, float x1, float y1, float x2, float y2), int err, const char* text, ...) {
    error_data* errorData = (error_data*) calloc(1, sizeof(error_data));
    if(errorData == NULL) {
        // No use trying to spawn another if we're out of memory.
        return NULL;
    }

    errorData->data = data;
    errorData->drawTop = drawTop;

    char textBuf[1024];
    va_list list;
    va_start(list, text);
    vsnprintf(textBuf, 1024, text, list);
    va_end(list);

    if(err < 0) {
        err = -err;
    }

    snprintf(errorData->fullText, 4096, "%s\nI/O Error: %s (%d)", textBuf, strerror(err), err);

    return prompt_display_notify("Error", errorData->fullText, COLOR_TEXT, errorData, error_draw_top, error_onresponse);
}