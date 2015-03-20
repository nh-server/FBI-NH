#include <ctrcommon/input.hpp>
#include <ctrcommon/platform.hpp>
#include <ctrcommon/ui.hpp>

#include <sstream>
#include <iomanip>
#include <stdio.h>

typedef enum {
    INSTALL,
    DELETE
} Mode;

int main(int argc, char **argv) {
    if(!platform_init()) {
        return 0;
    }

    std::vector<std::string> extensions;
    extensions.push_back("cia");

    MediaType destination = SD;
    Mode mode = INSTALL;
    bool netInstall = false;
    u64 freeSpace = fs_get_free_space(destination);
    auto onLoop = [&]() {
        bool breakLoop = false;
        if(input_is_pressed(BUTTON_L)) {
            if(destination == SD) {
                destination = NAND;
            } else {
                destination = SD;
            }

            freeSpace = fs_get_free_space(destination);
            if(mode == DELETE) {
                breakLoop = true;
            }
        }

        if(input_is_pressed(BUTTON_R)) {
            if(mode == INSTALL) {
                mode = DELETE;
            } else {
                mode = INSTALL;
            }

            breakLoop = true;
        }

        if(input_is_pressed(BUTTON_Y)) {
            netInstall = true;
            breakLoop = true;
        }

        std::stringstream stream;
        stream << "Free Space: " << freeSpace << " bytes (" << std::fixed << std::setprecision(2) << freeSpace / 1024.0f / 1024.0f << "MB)" << "\n";
        stream << "Destination: " << (destination == NAND ? "NAND" : "SD") << ", Mode: " << (mode == INSTALL ? "Install" : "Delete") << "\n";
        stream << "L - Switch Destination, R - Switch Mode" << "\n";
        stream << "Y - Receive an app over the network" << "\n";

        std::string str = stream.str();
        screen_draw_string(str, (screen_get_width() - screen_get_str_width(str)) / 2, screen_get_height() - 4 - screen_get_str_height(str), 255, 255, 255);

        return breakLoop;
    };

    auto onProgress = [&](int progress) {
        ui_display_progress(TOP_SCREEN, "Installing", "Press B to cancel.", true, progress);
        input_poll();
        return !input_is_pressed(BUTTON_B);
    };

    while(platform_is_running()) {
        std::string targetInstall;
        App targetDelete;
        bool obtained = false;
        if(mode == INSTALL) {
            obtained = ui_select_file(&targetInstall, "sdmc:", extensions, [&](bool inRoot) {
                return onLoop();
            });
        } else if(mode == DELETE) {
            obtained = ui_select_app(&targetDelete, destination, onLoop);
        }

        if(netInstall) {
            netInstall = false;

            screen_clear_buffers(BOTTOM_SCREEN, 0, 0, 0);

            RemoteFile file = ui_accept_remote_file(TOP_SCREEN);
            if(file.fd == NULL) {
                continue;
            }

            std::stringstream confirmStream;
            confirmStream << "Install the received application?" << "\n";
            confirmStream << "Size: " << file.fileSize << " bytes (" << std::fixed << std::setprecision(2) << file.fileSize / 1024.0f / 1024.0f << "MB)" << "\n";
            if(ui_prompt(TOP_SCREEN, confirmStream.str(), true)) {
                AppResult ret = app_install(destination, file.fd, file.fileSize, onProgress);
                std::stringstream resultMsg;
                if(mode == INSTALL) {
                    resultMsg << "Install ";
                } else if(mode == DELETE) {
                    resultMsg << "Delete ";
                }

                if(ret == APP_SUCCESS) {
                    resultMsg << "succeeded!";
                } else {
                    resultMsg << "failed!" << "\n";
                    resultMsg << app_get_result_string(ret) << "\n";
                }

                ui_prompt(TOP_SCREEN, resultMsg.str(), false);
            }

            fclose(file.fd);
            continue;
        }

        if(obtained) {
            std::stringstream prompt;
            if(mode == INSTALL) {
                prompt << "Install ";
            } else if(mode == DELETE) {
                prompt << "Delete ";
            }

            prompt << "the selected title?";
            if(ui_prompt(TOP_SCREEN, prompt.str(), true)) {
                AppResult ret = APP_SUCCESS;
                if(mode == INSTALL) {
                    ret = app_install_file(destination, targetInstall, onProgress);
                } else if(mode == DELETE) {
                    ui_display_message(TOP_SCREEN, "Deleting title...");
                    ret = app_delete(targetDelete);
                }

                std::stringstream resultMsg;
                if(mode == INSTALL) {
                    resultMsg << "Install ";
                } else if(mode == DELETE) {
                    resultMsg << "Delete ";
                }

                if(ret == APP_SUCCESS) {
                    resultMsg << "succeeded!";
                } else {
                    resultMsg << "failed!" << "\n";
                    resultMsg << app_get_result_string(ret) << "\n";
                }

                ui_prompt(TOP_SCREEN, resultMsg.str(), false);

                freeSpace = fs_get_free_space(destination);
            }
        }
    }

    platform_cleanup();
    return 0;
}
