#include <malloc.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <3ds.h>

#include "error.h"
#include "prompt.h"

typedef struct {
    char fullText[4096];
    void* data;
    void (*drawTop)(ui_view* view, void* data, float x1, float y1, float x2, float y2);
} error_data;

static const char* level_to_string(int level) {
    switch(level) {
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

static const char* summary_to_string(int summary) {
    switch(summary) {
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

static const char* module_to_string(int module) {
    switch(module) {
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

static const char* description_to_string(int description) {
    // TODO: Per-module descriptions.

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

static void error_draw_top(ui_view* view, void* data, float x1, float y1, float x2, float y2) {
    error_data* errorData = (error_data*) data;

    if(errorData->drawTop != NULL) {
        errorData->drawTop(view, errorData->data, x1, y1, x2, y2);
    }
}

static void error_onresponse(ui_view* view, void* data, bool response) {
    prompt_destroy(view);
    free(data);
}

void error_display(void* data, void (*drawTop)(ui_view* view, void* data, float x1, float y1, float x2, float y2), const char* text, ...) {
    error_data* errorData = (error_data*) calloc(1, sizeof(error_data));
    errorData->data = data;
    errorData->drawTop = drawTop;

    va_list list;
    va_start(list, text);
    vsnprintf(errorData->fullText, 4096, text, list);
    va_end(list);

    ui_push(prompt_create("Error", errorData->fullText, 0xFF000000, false, errorData, NULL, error_draw_top, error_onresponse));
}

void error_display_res(void* data, void (*drawTop)(ui_view* view, void* data, float x1, float y1, float x2, float y2), Result result, const char* text, ...) {
    error_data* errorData = (error_data*) calloc(1, sizeof(error_data));
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
    snprintf(errorData->fullText, 4096, "%s\nResult code: 0x%08lX\nLevel: %s (%d)\nSummary: %s (%d)\nModule: %s (%d)\nDecription: %s (%d)", textBuf, result, level_to_string(level), level, summary_to_string(summary), summary, module_to_string(module), module, description_to_string(description), description);

    ui_push(prompt_create("Error", errorData->fullText, 0xFF000000, false, errorData, NULL, error_draw_top, error_onresponse));
}

void error_display_errno(void* data, void (*drawTop)(ui_view* view, void* data, float x1, float y1, float x2, float y2), int err, const char* text, ...) {
    error_data* errorData = (error_data*) calloc(1, sizeof(error_data));
    errorData->data = data;
    errorData->drawTop = drawTop;

    char textBuf[1024];
    va_list list;
    va_start(list, text);
    vsnprintf(textBuf, 1024, text, list);
    va_end(list);

    snprintf(errorData->fullText, 4096, "%s\nError: %s (%d)", textBuf, strerror(err), err);

    ui_push(prompt_create("Error", errorData->fullText, 0xFF000000, false, errorData, NULL, error_draw_top, error_onresponse));
}