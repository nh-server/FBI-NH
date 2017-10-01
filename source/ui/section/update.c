#include <malloc.h>
#include <stdio.h>
#include <string.h>

#include <3ds.h>

#include "section.h"
#include "action/action.h"
#include "../error.h"
#include "../info.h"
#include "../prompt.h"
#include "../ui.h"
#include "../../core/screen.h"
#include "../../core/util.h"
#include "../../json/json.h"

static void update_check_update(ui_view* view, void* data, float* progress, char* text) {
    bool hasUpdate = false;
    char updateURL[INSTALL_URL_MAX];

    Result res = 0;
    u32 responseCode = 0;

    httpcContext context;
    if(R_SUCCEEDED(res = util_http_open(&context, &responseCode, "https://api.github.com/repos/Steveice10/FBI/releases/latest", true))) {
        u32 size = 0;
        if(R_SUCCEEDED(res = util_http_get_size(&context, &size))) {
            char* jsonText = (char*) calloc(sizeof(char), size);
            if(jsonText != NULL) {
                u32 bytesRead = 0;
                if(R_SUCCEEDED(res = util_http_read(&context, &bytesRead, (u8*) jsonText, size))) {
                    json_value* json = json_parse(jsonText, size);
                    if(json != NULL) {
                        if(json->type == json_object) {
                            json_value* name = NULL;
                            json_value* assets = NULL;

                            for(u32 i = 0; i < json->u.object.length; i++) {
                                json_value* val = json->u.object.values[i].value;
                                if(strncmp(json->u.object.values[i].name, "name", json->u.object.values[i].name_length) == 0 && val->type == json_string) {
                                    name = val;
                                } else if(strncmp(json->u.object.values[i].name, "assets", json->u.object.values[i].name_length) == 0 && val->type == json_array) {
                                    assets = val;
                                }
                            }

                            if(name != NULL && assets != NULL) {
                                char versionString[16];
                                snprintf(versionString, sizeof(versionString), "%d.%d.%d", VERSION_MAJOR, VERSION_MINOR, VERSION_MICRO);

                                if(strncmp(name->u.string.ptr, versionString, name->u.string.length) != 0) {
                                    char* url = NULL;

                                    for(u32 i = 0; i < assets->u.array.length; i++) {
                                        json_value* val = assets->u.array.values[i];
                                        if(val->type == json_object) {
                                            json_value* assetName = NULL;
                                            json_value* assetUrl = NULL;

                                            for(u32 j = 0; j < val->u.object.length; j++) {
                                                json_value* subVal = val->u.object.values[j].value;
                                                if(strncmp(val->u.object.values[j].name, "name", val->u.object.values[j].name_length) == 0 && subVal->type == json_string) {
                                                    assetName = subVal;
                                                } else if(strncmp(val->u.object.values[j].name, "browser_download_url", val->u.object.values[j].name_length) == 0 && subVal->type == json_string) {
                                                    assetUrl = subVal;
                                                }
                                            }

                                            if(assetName != NULL && assetUrl != NULL) {
                                                if(strncmp(assetName->u.string.ptr, util_get_3dsx_path() != NULL ? "FBI.3dsx" : "FBI.cia", assetName->u.string.length) == 0) {
                                                    url = assetUrl->u.string.ptr;
                                                    break;
                                                }
                                            }
                                        }
                                    }

                                    if(url != NULL) {
                                        strncpy(updateURL, url, INSTALL_URL_MAX);
                                        hasUpdate = true;
                                    } else {
                                        res = R_FBI_BAD_DATA;
                                    }
                                }
                            } else {
                                res = R_FBI_BAD_DATA;
                            }
                        } else {
                            res = R_FBI_BAD_DATA;
                        }

                        json_value_free(json);
                    } else {
                        res = R_FBI_PARSE_FAILED;
                    }
                }

                free(jsonText);
            } else {
                res = R_FBI_OUT_OF_MEMORY;
            }
        }

        Result closeRes = util_http_close(&context);
        if(R_SUCCEEDED(res)) {
            res = closeRes;
        }
    }

    ui_pop();
    info_destroy(view);

    if(hasUpdate) {
        action_install_url("Update FBI to the latest version?", updateURL, util_get_3dsx_path(), NULL, NULL, NULL);
    } else {
        if(R_FAILED(res)) {
            if(res == R_FBI_HTTP_RESPONSE_CODE) {
                error_display(NULL, NULL, "Failed to check for update.\nHTTP server returned response code %d", responseCode);
            } else {
                error_display_res(NULL, NULL, res, "Failed to check for update.");
            }
        } else {
            prompt_display_notify("Success", "No updates available.", COLOR_TEXT, NULL, NULL, NULL);
        }
    }
}

void update_open() {
    info_display("Checking For Updates", "", false, NULL, update_check_update, NULL);
}
