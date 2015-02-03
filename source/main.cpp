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

        if(obtained) {
            if(mode == INSTALL) {
                if(ui_prompt("Install the selected title?", true)) {
					ui_prompt(app_install(destination, targetInstall, onProgress) ? "Install succeeded!" : "Install failed!", false);
                }
            } else if(mode == DELETE) {
                if(ui_prompt("Delete the selected title?", true)) {
					ui_display_message("Deleting title...");
					ui_prompt(app_delete(targetDelete) ? "Delete succeeded!" : "Delete failed!", false);
                }
            }

			freeSpace = fs_get_free_space(destination);
        }
	}

	platform_cleanup();
	return 0;
}
