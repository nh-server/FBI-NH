#include <3ds.h>

#include "bnr.h"
#include "../stringutil.h"

static CFG_Language region_default_language[] = {
        CFG_LANGUAGE_JP,
        CFG_LANGUAGE_EN,
        CFG_LANGUAGE_EN,
        CFG_LANGUAGE_EN,
        CFG_LANGUAGE_ZH,
        CFG_LANGUAGE_KO,
        CFG_LANGUAGE_ZH
};

u16* bnr_select_title(BNR* bnr) {
    char title[0x100] = {'\0'};

    CFG_Language systemLanguage;
    if(R_SUCCEEDED(CFGU_GetSystemLanguage((u8*) &systemLanguage))) {
        utf16_to_utf8((uint8_t*) title, bnr->titles[systemLanguage], sizeof(title) - 1);
    }

    if(string_is_empty(title)) {
        CFG_Region systemRegion;
        if(R_SUCCEEDED(CFGU_SecureInfoGetRegion((u8*) &systemRegion))) {
            systemLanguage = region_default_language[systemRegion];
        } else {
            systemLanguage = CFG_LANGUAGE_JP;
        }
    }

    return bnr->titles[systemLanguage];
}