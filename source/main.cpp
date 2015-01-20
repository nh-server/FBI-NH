#include <stdlib.h>

#include "common.h"
#include "ui.h"

int main(int argc, char **argv) {
	platform_init();

	MediaType destination = SD;
	Mode mode = INSTALL;
	while(platform_is_running()) {
		char* targetInstall = NULL;
		App targetDelete;
		UIResult result;
		if(mode == INSTALL) {
			result = uiSelectFile(&targetInstall, "sdmc:", "cia", &destination, &mode);
		} else {
			result = uiSelectTitle(&targetDelete, &destination, &mode);
		}

		if(result == EXIT_APP) {
			break;
		} else if(result == SWITCH_MODE) {
			if(mode == INSTALL) {
				mode = DELETE;
			} else {
				mode = INSTALL;
			}
		} else {
			if(mode == INSTALL) {
				if(uiPromptOperation(mode, targetInstall)) {
					screen_clear_all();
					uiDisplayResult(true, app_install(destination, targetInstall, &uiDisplayInstallProgress));
				}

				free(targetInstall);
			} else {
				char* str = sdprintf("%08lx - %s, %s, %s", targetDelete.uniqueId, targetDelete.productCode, app_get_platform_name(targetDelete.platform), app_get_category_name(targetDelete.category));
				if(uiPromptOperation(mode, str)) {
					screen_clear_all();
					uiDisplayDeleting();
					uiDisplayResult(false, app_delete(destination, targetDelete));
				}

				free(str);
			}
		}
	}

	platform_cleanup();
	return 0;
}
