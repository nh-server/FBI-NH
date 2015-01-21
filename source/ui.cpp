#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <sys/dirent.h>
#include <vector>
#include <algorithm>

#include "ui.h"
#include "common.h"

struct ui_alphabetize {
    inline bool operator() (char* a, char* b) {
        return strcasecmp(a, b) < 0;
    }
};

bool uiIsDirectory(char* path) {
    DIR *dir = opendir(path);
    if(!dir) {
        return false;
    }

    closedir(dir);
    return true;
}

std::vector<char*>* uiGetDirectoryContents(const char* directory, const char* extensionFilter) {
    std::vector<char*>* contents = new std::vector<char*>();
    char slash[strlen(directory) + 2];
    snprintf(slash, sizeof(slash), "%s/", directory);
    DIR *dir = opendir(slash);
    if(dir != NULL) {
        while(true) {
            struct dirent *ent = readdir(dir);
            if(ent == NULL) {
                break;
            }

            char path[strlen(directory) + strlen(ent->d_name) + 2];
            snprintf(path, strlen(directory) + strlen(ent->d_name) + 2, "%s/%s", directory, ent->d_name);
            if(uiIsDirectory(path)) {
                contents->push_back(strdup(ent->d_name));
            } else {
                const char *dot = strrchr(path, '.');
                if(dot && dot != path && strcmp(dot + 1, extensionFilter) == 0) {
                    contents->push_back(strdup(ent->d_name));
                }
            }
        }

        closedir(dir);
        contents->push_back(strdup("."));
        contents->push_back(strdup(".."));
        std::sort(contents->begin(), contents->end(), ui_alphabetize());
    } else {
        return NULL;
    }

    return contents;
}

std::vector<char*>* uiTitlesToVector(App* apps, u32 count) {
    std::vector<char*>* contents = new std::vector<char*>();
    if(count == 0) {
        contents->push_back(strdup("None"));
    } else {
        for(u32 title = 0; title < count; title++) {
            contents->push_back(sdprintf("%08lx - %s, %s, %s", apps[title].uniqueId, apps[title].productCode, app_get_platform_name(apps[title].platform), app_get_category_name(apps[title].category)));
        }
    }

    std::sort(contents->begin(), contents->end(), ui_alphabetize());
    return contents;
}

void uiFreeVectorContents(std::vector<char*>* contents) {
    for(std::vector<char*>::iterator it = contents->begin(); it != contents->end(); it++) {
        free((char*) *it);
    }
}

UIResult uiDisplaySelector(char** selected, std::vector<char*>* contents, MediaType destination, Mode mode) {
    const char* selectCia = mode == INSTALL ? "Select a CIA to install." : "Select a CIA to delete.";
    const char* pressL = "Press L to switch destinations.";
    const char* pressR = "Press R to switch between installing and deleting.";
    const char* destString = destination == NAND ? "Destination: NAND" : "Destination: SD";
    const char* modeString = mode == INSTALL ? "Mode: Install" : "Mode: Delete";

    unsigned int cursor = 0;
    unsigned int scroll = 0;
    int horizScroll = 0;
    u64 horizEndTime = 0;
    UIResult result = SUCCESS;
    while(platform_is_running()) {
        input_poll();
        if(input_is_pressed(BUTTON_A)) {
            *selected = contents->at(cursor);
            result = SUCCESS;
            break;
        }

        if(input_is_pressed(BUTTON_B)) {
            result = BACK;
            break;
        }

        if(input_is_pressed(BUTTON_L)) {
            result = SWITCH_DEST;
            break;
        }

        if(input_is_pressed(BUTTON_R)) {
            result = SWITCH_MODE;
            break;
        }

        if(input_is_pressed(BUTTON_DOWN) && cursor < contents->size() - 1) {
            cursor++;
            int diff = cursor - scroll;
            if(diff >= 20) {
                scroll++;
            }

            horizScroll = 0;
            horizEndTime = 0;
        }

        if(input_is_pressed(BUTTON_UP) && cursor > 0) {
            cursor--;
            int diff = cursor - scroll;
            if(diff < 0) {
                scroll--;
            }

            horizScroll = 0;
            horizEndTime = 0;
        }

        screen_begin_draw();
        screen_clear(0, 0, 0);

        int screenWidth = screen_get_width();
        int i = 0;
        for(std::vector<char*>::iterator it = contents->begin() + scroll; it != contents->end(); it++) {
            u8 color = 255;
            int offset = 0;
            if(i + scroll == cursor) {
                screen_fill(0, i * 12, screenWidth, 8, 255, 255, 255);
                color = 0;
                int width = strlen(*it) * 8;
                if(width > screenWidth) {
                    if(-horizScroll + screenWidth >= width) {
                        if(horizEndTime == 0) {
                            horizEndTime = platform_get_time();
                        } else if(platform_get_time() - horizEndTime >= 4000) {
                            horizScroll = 0;
                            horizEndTime = 0;
                        }
                    } else {
                        horizScroll -= 1;
                    }
                }

                offset = horizScroll;
            }

            screen_draw_string(*it, offset, i * 12, color, color, color);
            i++;
            if(i >= 20) {
                break;
            }
        }

        screen_end_draw();

        screen_begin_draw_info();
        screen_clear(0, 0, 0);

        screen_draw_string(selectCia, (screen_get_width() - screen_get_str_width(selectCia)) / 2, (screen_get_height() - screen_get_str_height(pressL)) / 2 - screen_get_str_height(selectCia), 255, 255, 255);
        screen_draw_string(pressL, (screen_get_width() - screen_get_str_width(pressL)) / 2, (screen_get_height() - screen_get_str_height(pressL)) / 2, 255, 255, 255);
        screen_draw_string(pressR, (screen_get_width() - screen_get_str_width(pressR)) / 2, (screen_get_height() - screen_get_str_height(pressL)) / 2 + screen_get_str_height(pressR), 255, 255, 255);

        screen_draw_string(destString, 0, screen_get_height() - screen_get_str_height(destString), 255, 255, 255);
        screen_draw_string(modeString, screen_get_width() - screen_get_str_width(modeString), screen_get_height() - screen_get_str_height(modeString), 255, 255, 255);
        screen_end_draw();

        screen_swap_buffers();
    }

    if(!platform_is_running()) {
        result = EXIT_APP;
    }

    return result;
}

UIResult uiSelectFile(char** selected, const char* directory, const char* extension, MediaType* destination, Mode* mode) {
    std::vector<char*>* contents = uiGetDirectoryContents(directory, extension);
    UIResult result;
    while(true) {
        char* selectedEntry = NULL;
        UIResult res = uiDisplaySelector(&selectedEntry, contents, *destination, *mode);
        if(res == SWITCH_DEST) {
            if(*destination == NAND) {
                *destination = SD;
            } else {
                *destination = NAND;
            }

            continue;
        } else if(res == BACK || (selectedEntry != NULL && strcmp(selectedEntry, "..") == 0)) {
            if(strcmp(directory, "sdmc:") != 0) {
                result = BACK;
                break;
            } else {
                continue;
            }
        } else if(res != SUCCESS) {
            result = res;
            break;
        }

        if(strcmp(selectedEntry, ".") == 0) {
            continue;
        }

        char* path = (char*) malloc(strlen(directory) + strlen(selectedEntry) + 2);
        snprintf(path, strlen(directory) + strlen(selectedEntry) + 2, "%s/%s", directory, selectedEntry);
        if(uiIsDirectory(path)) {
            char *select;
            UIResult dirRes = uiSelectFile(&select, path, extension, destination, mode);
            free(path);
            if(dirRes == BACK) {
                continue;
            }

            result = dirRes;
            *selected = select;
            break;
        } else {
            result = SUCCESS;
            *selected = path;
            break;
        }
    }

    uiFreeVectorContents(contents);
    delete(contents);
    return result;
}

UIResult uiSelectTitle(App* selected, MediaType* destination, Mode* mode) {
    u32 appCount;
    App* apps = app_list(*destination, &appCount);
    std::vector<char*>* contents = uiTitlesToVector(apps, appCount);
    UIResult result;
    while(true) {
        char* selectedEntry = NULL;
        UIResult res = uiDisplaySelector(&selectedEntry, contents, *destination, *mode);
        if(selectedEntry != NULL && strcmp(selectedEntry, "None") == 0) {
            continue;
        }

        if(res == BACK) {
            continue;
        } else if(res == SWITCH_DEST) {
            if(*destination == NAND) {
                *destination = SD;
            } else {
                *destination = NAND;
            }

            uiFreeVectorContents(contents);
            delete(contents);
            free(apps);
            apps = app_list(*destination, &appCount);
            contents = uiTitlesToVector(apps, appCount);
            continue;
        } else if(res != SUCCESS) {
            result = res;
            break;
        }

        for(u32 i = 0; i < appCount; i++) {
            char* data = sdprintf("%08lx - %s, %s, %s", apps[i].uniqueId, apps[i].productCode, app_get_platform_name(apps[i].platform), app_get_category_name(apps[i].category));
            if(strcmp(selectedEntry, data) == 0) {
                *selected = apps[i];
                free(data);
                break;
            }

            free(data);
        }

        if(selected == NULL) {
            continue;
        }

        result = SUCCESS;
        break;
    }

    uiFreeVectorContents(contents);
    delete(contents);
    free(apps);
    return result;
}

bool uiDisplayInstallProgress(int progress) {
    char* msg = sdprintf("Installing: [                         ] %03d%%", progress);
    const char* cancel = "Press B to cancel.";
    for(int pos = 13; pos < 13 + (progress / 4); pos++) {
        msg[pos] = '|';
    }

    screen_begin_draw_info();
    screen_clear(0, 0, 0);
    screen_draw_string(msg, (screen_get_width() - screen_get_str_width(msg)) / 2, (screen_get_height() - screen_get_str_height(msg)) / 2 - screen_get_str_height(msg), 255, 255, 255);
    screen_draw_string(cancel, (screen_get_width() - screen_get_str_width(cancel)) / 2, (screen_get_height() - screen_get_str_height(cancel)) / 2 + screen_get_str_height(cancel), 255, 255, 255);
    screen_end_draw();
    screen_swap_buffers_quick();

    free(msg);

    input_poll();
    return !input_is_pressed(BUTTON_B);
}

void uiDisplayDeleting() {
    const char* msg = "Deleting title...";
    screen_begin_draw_info();
    screen_clear(0, 0, 0);
    screen_draw_string(msg, (screen_get_width() - screen_get_str_width(msg)) / 2, (screen_get_height() - screen_get_str_height(msg)) / 2, 255, 255, 255);
    screen_end_draw();
    screen_swap_buffers();
}

void uiDisplayResult(bool install, bool state) {
    const char* msg = install ? (state ? "Install succeeded! Press start." : "Install failed! Press start.") : (state ? "Delete succeeded! Press start." : "Delete failed! Press start.");
    while(platform_is_running()) {
        input_poll();
        if(input_is_pressed(BUTTON_START)) {
            break;
        }

        screen_begin_draw_info();
        screen_clear(0, 0, 0);
        screen_draw_string(msg, (screen_get_width() - screen_get_str_width(msg)) / 2, (screen_get_height() - screen_get_str_height(msg)) / 2, 255, 255, 255);
        screen_end_draw();
        screen_swap_buffers();
    }
}

bool uiPromptOperation(Mode mode) {
    char* msg = sdprintf("%s the selected title?", mode == INSTALL ? "Install" : "Delete");
    const char* prompt = "Press A to confirm, B to cancel.";
    while(platform_is_running()) {
        input_poll();
        if(input_is_pressed(BUTTON_A)) {
            free(msg);
            return true;
        }

        if(input_is_pressed(BUTTON_B)) {
            free(msg);
            return false;
        }

        screen_begin_draw_info();
        screen_clear(0, 0, 0);
        screen_draw_string(msg, (screen_get_width() - screen_get_str_width(msg)) / 2, (screen_get_height() - screen_get_str_height(msg)) / 2 - screen_get_str_height(msg), 255, 255, 255);
        screen_draw_string(prompt, (screen_get_width() - screen_get_str_width(prompt)) / 2, (screen_get_height() - screen_get_str_height(prompt)) / 2 + screen_get_str_height(prompt), 255, 255, 255);
        screen_end_draw();
        screen_swap_buffers();
    }

    free(msg);
    return false;
}
