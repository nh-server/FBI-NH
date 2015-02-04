#include <ctrcommon/common.hpp>

#include <sstream>
#include <iomanip>

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
        ui_display_progress("Installing", "Press B to cancel.", true, progress);
        input_poll();
        return !input_is_pressed(BUTTON_B);
    };

    while(platform_is_running()) {
        std::string targetInstall;
        App targetDelete;
        bool obtained = false;
        if(mode == INSTALL) {
            obtained = ui_select_file(&targetInstall, "sdmc:", extensions, onLoop);
        } else if(mode == DELETE) {
            obtained = ui_select_app(&targetDelete, destination, onLoop);
        }

        if(netInstall) {
            netInstall = false;

            // Clear bottom screen on both buffers.
            screen_begin_draw(BOTTOM_SCREEN);
            screen_clear(0, 0, 0);
            screen_end_draw();
            screen_swap_buffers();

            screen_begin_draw(BOTTOM_SCREEN);
            screen_clear(0, 0, 0);
            screen_end_draw();
            screen_swap_buffers();

            RemoteFile file = ui_accept_remote_file();
            if(file.socket == -1) {
                continue;
            }

            std::stringstream confirmStream;
            confirmStream << "Install the received application?" << "\n";
            confirmStream << "Size: " << file.fileSize << " bytes (" << std::fixed << std::setprecision(2) << file.fileSize / 1024.0f / 1024.0f << "MB)" << "\n";
            if(ui_prompt(confirmStream.str(), true)) {
                int ret = app_install(destination, file.socket, true, file.fileSize, onProgress);
                std::stringstream resultMsg;
                resultMsg << "Install ";
                if(ret == 0) {
                    resultMsg << "succeeded!";
                } else if(ret == -2) {
                    resultMsg << "cancelled!";
                } else {
                    resultMsg << "failed! Error: 0x" << std::hex << ret;
                }

                ui_prompt(resultMsg.str(), false);
            }

            socket_close(file.socket);
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
            if(ui_prompt(prompt.str(), true)) {
                int ret = 0;
                if(mode == INSTALL) {
                    ret = app_install_file(destination, targetInstall, onProgress);
                } else if(mode == DELETE) {
                    ui_display_message("Deleting title...");
                    ret = app_delete(targetDelete);
                }

                std::stringstream resultMsg;
                if(mode == INSTALL) {
                    resultMsg << "Install ";
                } else if(mode == DELETE) {
                    resultMsg << "Delete ";
                }

                if(ret == 0) {
                    resultMsg << "succeeded!";
                } else if(ret == -2) {
                    resultMsg << "cancelled!";
                } else {
                    resultMsg << "failed! Error: 0x" << std::hex << ret;
                }

                ui_prompt(resultMsg.str(), false);

                freeSpace = fs_get_free_space(destination);
            }
        }
    }

    platform_cleanup();
    return 0;
}
