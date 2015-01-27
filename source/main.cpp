#include "common.hpp"

#include <sstream>
#include <iomanip>

typedef enum {
    INSTALL,
    DELETE
} Mode;

bool ui_display_install_progress(int progress) {
    std::stringstream stream;
    stream << "Installing: [";
    int progressBars = progress / 4;
    for(int i = 0; i < 25; i++) {
        if(i < progressBars) {
            stream << '|';
        } else {
            stream << ' ';
        }
    }

    std::ios state(NULL);
    state.copyfmt(stream);
    stream << "] " << std::setfill('0') << std::setw(3) << progress;
    stream.copyfmt(state);
    stream << "%";

    std::string msg = stream.str();
    std::string cancel = "Press B to cancel.";

    screen_begin_draw(TOP_SCREEN);
    screen_clear(0, 0, 0);
    screen_draw_string(msg, (screen_get_width() - screen_get_str_width(msg)) / 2, (screen_get_height() - screen_get_str_height(msg)) / 2 - screen_get_str_height(msg), 255, 255, 255);
    screen_draw_string(cancel, (screen_get_width() - screen_get_str_width(cancel)) / 2, (screen_get_height() - screen_get_str_height(cancel)) / 2 + screen_get_str_height(cancel), 255, 255, 255);
    screen_end_draw();
    screen_swap_buffers_quick();;

    input_poll();
    return !input_is_pressed(BUTTON_B);
}

void ui_display_deleting() {
    std::string msg = "Deleting title...";
    screen_begin_draw(TOP_SCREEN);
    screen_clear(0, 0, 0);
    screen_draw_string(msg, (screen_get_width() - screen_get_str_width(msg)) / 2, (screen_get_height() - screen_get_str_height(msg)) / 2, 255, 255, 255);
    screen_end_draw();
    screen_swap_buffers();
}

void ui_display_result(bool install, bool state) {
    std::string msg = install ? (state ? "Install succeeded! Press start." : "Install failed! Press start.") : (state ? "Delete succeeded! Press start." : "Delete failed! Press start.");
    while(platform_is_running()) {
        input_poll();
        if(input_is_pressed(BUTTON_START)) {
            break;
        }

        screen_begin_draw(TOP_SCREEN);
        screen_clear(0, 0, 0);
        screen_draw_string(msg, (screen_get_width() - screen_get_str_width(msg)) / 2, (screen_get_height() - screen_get_str_height(msg)) / 2, 255, 255, 255);
        screen_end_draw();
        screen_swap_buffers();
    }
}

bool ui_prompt_operation(Mode mode, std::string name) {
    std::stringstream stream;
    stream << (mode == INSTALL ? "Install" : "Delete") << " the selected title?";
    std::string msg = stream.str();
    std::string prompt = "Press A to confirm, B to cancel.";
    while(platform_is_running()) {
        input_poll();
        if(input_is_pressed(BUTTON_A)) {
            return true;
        }

        if(input_is_pressed(BUTTON_B)) {
            return false;
        }

        screen_begin_draw(TOP_SCREEN);
        screen_clear(0, 0, 0);
        screen_draw_string(msg, (screen_get_width() - screen_get_str_width(msg)) / 2, (screen_get_height() - screen_get_str_height(msg)) / 2 - screen_get_str_height(msg), 255, 255, 255);
        screen_draw_string(name, (screen_get_width() - screen_get_str_width(name)) / 2, (screen_get_height() - screen_get_str_height(name)) / 2, 255, 255, 255);
        screen_draw_string(prompt, (screen_get_width() - screen_get_str_width(prompt)) / 2, (screen_get_height() - screen_get_str_height(prompt)) / 2 + screen_get_str_height(prompt), 255, 255, 255);
        screen_end_draw();
        screen_swap_buffers();
    }

    return false;
}

int main(int argc, char **argv) {
	if(!platform_init()) {
		return 0;
	}

	MediaType destination = SD;
	Mode mode = INSTALL;
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

        std::stringstream stream;
        stream << "Free Space: " << freeSpace << " bytes (" << std::fixed << std::setprecision(2) << freeSpace / 1024.0f / 1024.0f << "MB)";

        std::string space = stream.str();
        std::string status = std::string("Destination: ") + (destination == NAND ? "NAND" : "SD") + ", Mode: " + (mode == INSTALL ? "Install" : "Delete");
        std::string msg = "L - Switch Destination, R - Switch Mode";

        screen_draw_string(space, (screen_get_width() - screen_get_str_width(space)) / 2, screen_get_height() - 4 - screen_get_str_height(msg) - screen_get_str_height(status) - screen_get_str_height(space), 255, 255, 255);
        screen_draw_string(status, (screen_get_width() - screen_get_str_width(status)) / 2, screen_get_height() - 4 - screen_get_str_height(msg) - screen_get_str_height(status), 255, 255, 255);
        screen_draw_string(msg, (screen_get_width() - screen_get_str_width(msg)) / 2, screen_get_height() - 4 - screen_get_str_height(msg), 255, 255, 255);

        return breakLoop;
    };

	while(platform_is_running()) {
        std::string targetInstall;
		App targetDelete;
        bool obtained = false;
		if(mode == INSTALL) {
			obtained = ui_select_file("sdmc:", "cia", &targetInstall, onLoop);
		} else if(mode == DELETE) {
            obtained = ui_select_app(destination, &targetDelete, onLoop);
		}

        if(obtained) {
            if(mode == INSTALL) {
                if(ui_prompt_operation(mode, targetInstall)) {
                    ui_display_result(true, app_install(destination, targetInstall, &ui_display_install_progress));
                }
            } else if(mode == DELETE) {
                if(ui_prompt_operation(mode, targetDelete.productCode)) {
                    ui_display_deleting();
                    ui_display_result(false, app_delete(targetDelete));
                }
            }
        }
	}

	platform_cleanup();
	return 0;
}
