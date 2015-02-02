#include <ctrcommon/common.hpp>

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
    stream << "%" << "\n";
	stream << "Press B to cancel." << "\n";

    std::string str = stream.str();

    screen_begin_draw(TOP_SCREEN);
    screen_clear(0, 0, 0);
    screen_draw_string(str, (screen_get_width() - screen_get_str_width(str)) / 2, (screen_get_height() - screen_get_str_height(str)) / 2, 255, 255, 255);
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

bool ui_prompt_operation(Mode mode) {
    std::stringstream stream;
    stream << (mode == INSTALL ? "Install" : "Delete") << " the selected title?" << "\n";
    stream << "Press A to confirm, B to cancel." << "\n";
	std::string str = stream.str();
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
        screen_draw_string(str, (screen_get_width() - screen_get_str_width(str)) / 2, (screen_get_height() - screen_get_str_height(str)) / 2, 255, 255, 255);
        screen_end_draw();
        screen_swap_buffers();
    }

    return false;
}

int main(int argc, char **argv) {
	if(!platform_init()) {
		return 0;
	}

	std::vector<std::string> extensions;
	extensions.push_back("cia");

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
        stream << "Free Space: " << freeSpace << " bytes (" << std::fixed << std::setprecision(2) << freeSpace / 1024.0f / 1024.0f << "MB)" << "\n";
        stream << "Destination: " << (destination == NAND ? "NAND" : "SD") << ", Mode: " << (mode == INSTALL ? "Install" : "Delete") << "\n";
        stream << "L - Switch Destination, R - Switch Mode" << "\n";

        std::string str = stream.str();
        screen_draw_string(str, (screen_get_width() - screen_get_str_width(str)) / 2, screen_get_height() - 4 - screen_get_str_height(str), 255, 255, 255);

        return breakLoop;
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

        if(obtained) {
            if(mode == INSTALL) {
                if(ui_prompt_operation(mode)) {
                    ui_display_result(true, app_install(destination, targetInstall, &ui_display_install_progress));
                }
            } else if(mode == DELETE) {
                if(ui_prompt_operation(mode)) {
                    ui_display_deleting();
                    ui_display_result(false, app_delete(targetDelete));
                }
            }

			freeSpace = fs_get_free_space(destination);
        }
	}

	platform_cleanup();
	return 0;
}
