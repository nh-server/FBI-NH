#ifndef __UI_H__
#define __UI_H__

#include "common.h"

typedef enum _ui_result {
    SUCCESS,
    BACK,
    SWITCH_DEST,
    SWITCH_MODE,
    EXIT_APP
} UIResult;

typedef enum _mode {
    INSTALL,
    DELETE
} Mode;

UIResult uiSelectFile(char** selected, const char* directory, const char* extension, MediaType* destination, Mode* mode);
UIResult uiSelectTitle(App* selected, MediaType* destination, Mode* mode);
bool uiDisplayInstallProgress(int progress);
void uiDisplayDeleting();
void uiDisplayResult(bool install, bool state);
bool uiPromptOperation(Mode mode);

#endif
