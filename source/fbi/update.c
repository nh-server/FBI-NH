#include <malloc.h>
#include <stdio.h>
#include <string.h>

#include <3ds.h>
#include <jansson.h>

#include "resources.h"
#include "section.h"
#include "action/action.h"
#include "task/uitask.h"
#include "../core/core.h"

typedef struct {
    u32 id;
    bool cia;
} update_data;

static void update_finished_all(void* data) {
    free(data);
}

static void update_check_update(ui_view* view, void* data, float* progress, char* text) {
    update_data* updateData = (update_data*) data;

    bool hasUpdate = false;
    char updateURL[DOWNLOAD_URL_MAX];

    Result res = 0;

    json_t* json = NULL;
    if(R_SUCCEEDED(res = http_download_json("https://api.titledb.com/v1/entry?nested=true&only=id"
                                                    "&only=cia.id&only=cia.version&only=cia.updated_at"
                                                    "&only=tdsx.id&only=tdsx.version&only=tdsx.updated_at"
                                                    "&_filters=%7B%22id%22%3A%20%22138%22%7D", &json, 16 * 1024))) {
        const char* type = fs_get_3dsx_path() != NULL ? "tdsx" : "cia";

        json_t* entry = NULL;
        json_t* idJson = NULL;
        json_t* objs = NULL;
        if(json_is_array(json) && json_array_size(json) == 1
           && json_is_object(entry = json_array_get(json, 0))
           && json_is_integer(idJson = json_object_get(entry, "id"))
           && json_is_array(objs = json_object_get(entry, type))) {
            if(json_array_size(json) > 0) {
                updateData->id = (u32) json_integer_value(idJson);
                updateData->cia = fs_get_3dsx_path() != NULL;

                u32 id = 0;
                u32 latestMajor = 0;
                u32 latestMinor = 0;
                u32 latestMicro = 0;

                for(u32 i = 0; i < json_array_size(objs); i++) {
                    json_t* obj = json_array_get(objs, i);
                    if(json_is_object(obj)) {
                        json_t* subIdJson = json_object_get(obj, "id");
                        json_t* versionJson = json_object_get(obj, "version");
                        if(json_is_integer(subIdJson) && json_is_string(versionJson)) {
                            u32 subId = (u32) json_integer_value(subIdJson);
                            const char* version = json_string_value(versionJson);

                            u32 major = 0;
                            u32 minor = 0;
                            u32 micro = 0;
                            sscanf(version, "%lu.%lu.%lu", &major, &minor, &micro);

                            if(major > latestMajor
                               || (major == latestMajor && minor > latestMinor)
                               || (major == latestMajor && minor == latestMinor && micro > latestMicro)) {
                                id = subId;
                                latestMajor = major;
                                latestMinor = minor;
                                latestMicro = micro;
                            }
                        }
                    }
                }

                if(latestMajor > VERSION_MAJOR
                   || (latestMajor == VERSION_MAJOR && latestMinor > VERSION_MINOR)
                   || (latestMajor == VERSION_MAJOR && latestMinor == VERSION_MINOR && latestMicro > VERSION_MICRO)) {
                    snprintf(updateURL, DOWNLOAD_URL_MAX, "https://3ds.titledb.com/v1/%s/%lu/download", type, id);
                    hasUpdate = true;
                }
            }
        } else {
            res = R_APP_BAD_DATA;
        }

        json_decref(json);
    }

    ui_pop();
    info_destroy(view);

    if(hasUpdate) {
        action_install_url("Update FBI to the latest version?", updateURL, fs_get_3dsx_path(), updateData, NULL, update_finished_all, NULL);
    } else {
        if(R_FAILED(res)) {
            error_display_res(NULL, NULL, res, "Failed to check for update.");
        } else {
            prompt_display_notify("Success", "No updates available.", COLOR_TEXT, NULL, NULL, NULL);
        }

        free(updateData);
    }
}

void update_open() {
    update_data* data = (update_data*) calloc(1, sizeof(update_data));
    if(data == NULL) {
        error_display(NULL, NULL, "Failed to allocate update data.");

        return;
    }

    info_display("Checking For Updates", "", false, data, update_check_update, NULL);
}
