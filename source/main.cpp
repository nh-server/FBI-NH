#include <ctrcommon/app.hpp>
#include <ctrcommon/fs.hpp>
#include <ctrcommon/gpu.hpp>
#include <ctrcommon/input.hpp>
#include <ctrcommon/nor.hpp>
#include <ctrcommon/platform.hpp>
#include <ctrcommon/ui.hpp>

#include "rop.h"

#include <sys/errno.h>
#include <stdio.h>
#include <string.h>

#include <sstream>
#include <iomanip>

typedef enum {
    INSTALL_CIA,
    DELETE_CIA,
    DELETE_TITLE,
    LAUNCH_TITLE
} Mode;

std::vector<std::string> extensions = {"cia"};

bool exit = false;
bool showNetworkPrompts = true;
u64 freeSpace = 0;
MediaType destination = SD;
Mode mode = INSTALL_CIA;

int prevProgress = -1;
std::string installInfo = "";

bool onProgress(u64 pos, u64 totalSize) {
    u32 progress = (u32) ((pos * 100) / totalSize);
    if(prevProgress != (int) progress) {
        prevProgress = (int) progress;

        std::stringstream details;
        details << installInfo;
        details << "Press B to cancel.";

        uiDisplayProgress(TOP_SCREEN, "Installing", details.str(), true, progress);
    }

    inputPoll();
    return !inputIsPressed(BUTTON_B);
}

void networkInstall() {
    while(platformIsRunning()) {
        gpuClearScreens();

        RemoteFile file = uiAcceptRemoteFile(TOP_SCREEN, [&](std::stringstream& infoStream) {
            if(inputIsPressed(BUTTON_A)) {
                showNetworkPrompts = !showNetworkPrompts;
            }

            infoStream << "\n";
            infoStream << "Prompts: " << (showNetworkPrompts ? "Enabled" : "Disabled") << "\n";
            infoStream << "Press A to toggle prompts.";
        });

        if(file.fd == NULL) {
            break;
        }

        std::stringstream confirmStream;
        confirmStream << "Install the received application?" << "\n";
        confirmStream << "Size: " << file.fileSize << " bytes (" << std::fixed << std::setprecision(2) << file.fileSize / 1024.0f / 1024.0f << "MB)";
        if(!showNetworkPrompts || uiPrompt(TOP_SCREEN, confirmStream.str(), true)) {
            AppResult ret = appInstall(destination, file.fd, file.fileSize, &onProgress);
            prevProgress = -1;
            if(showNetworkPrompts || ret != APP_SUCCESS) {
                std::stringstream resultMsg;
                resultMsg << "Install ";
                if(ret == APP_SUCCESS) {
                    resultMsg << "succeeded!";
                } else {
                    resultMsg << "failed!" << "\n";
                    resultMsg << appGetResultString(ret);
                }

                uiPrompt(TOP_SCREEN, resultMsg.str(), false);

                freeSpace = fsGetFreeSpace(destination);
            }
        }

        fclose(file.fd);
    }
}

void installROP() {
    u32 selected = 0;
    bool dirty = true;
    while(platformIsRunning()) {
        inputPoll();
        if(inputIsPressed(BUTTON_B)) {
            break;
        }

        if(inputIsPressed(BUTTON_A)) {
            std::stringstream stream;
            stream << "Install the selected ROP?" << "\n";
            stream << ropNames[selected];

            if(uiPrompt(TOP_SCREEN, stream.str(), true)) {
                u16 userSettingsOffset = 0;
                bool result = norRead(0x20, &userSettingsOffset, 2) && norWrite(userSettingsOffset << 3, rops[selected], ROP_SIZE);

                std::stringstream resultMsg;
                resultMsg << "ROP installation ";
                if(result) {
                    resultMsg << "succeeded!";
                } else {
                    resultMsg << "failed!" << "\n";
                    resultMsg << platformGetErrorString(platformGetError());
                }

                uiPrompt(TOP_SCREEN, resultMsg.str(), false);
            }

            dirty = true;
        }

        if(inputIsPressed(BUTTON_LEFT)) {
            if(selected == 0) {
                selected = ROP_COUNT - 1;
            } else {
                selected--;
            }

            dirty = true;
        }

        if(inputIsPressed(BUTTON_RIGHT)) {
            if(selected >= ROP_COUNT - 1) {
                selected = 0;
            } else {
                selected++;
            }

            dirty = true;
        }

        if(dirty) {
            std::stringstream stream;
            stream << "Select a ROP to install." << "\n";
            stream << "< " << ropNames[selected] << " >" << "\n";
            stream << "Press A to install, B to cancel.";
            uiDisplayMessage(TOP_SCREEN, stream.str());
        }
    }
}

bool installCIA(MediaType destination, const std::string path, const std::string fileName) {
    std::string name = fileName;
    if(name.length() > 40) {
        name.resize(40);
        name += "...";
    }

    std::stringstream batchInstallStream;
    batchInstallStream << name << "\n";
    installInfo = batchInstallStream.str();

    AppResult ret = appInstallFile(destination, path, &onProgress);
    prevProgress = -1;
    installInfo = "";

    if(ret != APP_SUCCESS && platformHasError()) {
        Error error = platformGetError();
        if(error.module == MODULE_NN_AM && error.description == DESCRIPTION_ALREADY_EXISTS) {
            std::stringstream overwriteMsg;
            overwriteMsg << "Title already installed, overwrite?" << "\n";
            overwriteMsg << name;
            if(uiPrompt(TOP_SCREEN, overwriteMsg.str(), true)) {
                uiDisplayMessage(TOP_SCREEN, "Deleting title...");
                AppResult deleteRet = appDelete(appGetCiaInfo(path, destination));
                if(deleteRet != APP_SUCCESS) {
                    std::stringstream resultMsg;
                    resultMsg << "Delete failed!" << "\n";
                    resultMsg << name << "\n";
                    resultMsg << appGetResultString(deleteRet);
                    uiPrompt(TOP_SCREEN, resultMsg.str(), false);

                    return false;
                }

                ret = appInstallFile(destination, path, &onProgress);
                prevProgress = -1;
                installInfo = "";
            } else {
                platformSetError(error);
            }
        } else {
            platformSetError(error);
        }
    }

    if(ret != APP_SUCCESS) {
        std::stringstream resultMsg;
        resultMsg << "Install failed!" << "\n";
        resultMsg << name << "\n";
        resultMsg << appGetResultString(ret);
        uiPrompt(TOP_SCREEN, resultMsg.str(), false);

        return false;
    }

    return true;
}

bool deleteCIA(const std::string path, const std::string fileName) {
    std::string name = fileName;
    if(name.length() > 40) {
        name.resize(40);
        name += "...";
    }

    std::stringstream deleteStream;
    deleteStream << "Deleting CIA..." << "\n";
    deleteStream << name;
    uiDisplayMessage(TOP_SCREEN, deleteStream.str());

    if(remove(path.c_str()) != 0) {
        std::stringstream resultMsg;
        resultMsg << "Delete failed!" << "\n";
        resultMsg << name << "\n";
        resultMsg << strerror(errno);
        uiPrompt(TOP_SCREEN, resultMsg.str(), false);

        return false;
    }

    return true;
}

bool deleteTitle(App app) {
    uiDisplayMessage(TOP_SCREEN, "Deleting title...");
    AppResult ret = appDelete(app);

    if(ret != APP_SUCCESS) {
        std::stringstream resultMsg;
        resultMsg << "Delete failed!" << "\n";
        resultMsg << appGetResultString(ret);
        uiPrompt(TOP_SCREEN, resultMsg.str(), false);

        return false;
    }

    return true;
}

bool launchTitle(App app) {
    uiDisplayMessage(TOP_SCREEN, "Launching title...");
    AppResult ret = appLaunch(app);

    if(ret != APP_SUCCESS) {
        std::stringstream resultMsg;
        resultMsg << "Launch failed!" << "\n";
        resultMsg << appGetResultString(ret);
        uiPrompt(TOP_SCREEN, resultMsg.str(), false);

        return false;
    }

    return true;
}

bool onLoop() {
    bool launcher = platformHasLauncher();
    if(launcher && inputIsPressed(BUTTON_START)) {
        exit = true;
        return true;
    }

    bool breakLoop = false;

    if(inputIsPressed(BUTTON_L)) {
        if(destination == SD) {
            destination = NAND;
        } else {
            destination = SD;
        }

        freeSpace = fsGetFreeSpace(destination);
        if(mode == DELETE_TITLE || mode == LAUNCH_TITLE) {
            breakLoop = true;
        }
    }

    if(inputIsPressed(BUTTON_R)) {
        if(mode == INSTALL_CIA) {
            mode = DELETE_CIA;
        } else if(mode == DELETE_CIA) {
            mode = DELETE_TITLE;
            breakLoop = true;
        } else if(mode == DELETE_TITLE) {
            mode = LAUNCH_TITLE;
        } else if(mode == LAUNCH_TITLE) {
            mode = INSTALL_CIA;
            breakLoop = true;
        }
    }

    if(mode == INSTALL_CIA && inputIsPressed(BUTTON_Y)) {
        networkInstall();
    }

    if(inputIsPressed(BUTTON_SELECT)) {
        installROP();
    }

    std::stringstream stream;
    stream << "Battery: ";
    if(platformIsBatteryCharging()) {
        stream << "Charging";
    } else {
        for(u32 i = 0; i < platformGetBatteryLevel(); i++) {
            stream << "|";
        }
    }

    stream << ", WiFi: ";
    if(!platformIsWifiConnected()) {
        stream << "Disconnected";
    } else {
        for(u32 i = 0; i < platformGetWifiLevel(); i++) {
            stream << "|";
        }
    }

    stream << "\n";

    stream << "Free Space: " << freeSpace << " bytes (" << std::fixed << std::setprecision(2) << freeSpace / 1024.0f / 1024.0f << "MB)" << "\n";
    stream << "Destination: " << (destination == NAND ? "NAND" : "SD") << ", Mode: " << (mode == INSTALL_CIA ? "Install CIA" : mode == DELETE_CIA ? "Delete CIA" : mode == DELETE_TITLE ? "Delete Title" : "Launch Title") << "\n";
    stream << "L - Switch Destination, R - Switch Mode" << "\n";
    if(mode == INSTALL_CIA) {
        stream << "X - Install all CIAs in the current directory" << "\n";
        stream << "Y - Receive an app over the network" << "\n";
    } else if(mode == DELETE_CIA) {
        stream << "X - Delete all CIAs in the current directory" << "\n";
    }

    stream << "SELECT - Install MSET ROP";

    if(launcher) {
        stream << "\n" << "START - Exit to launcher";
    }

    std::string str = stream.str();
    const std::string title = "FBI v1.4.6";
    gputDrawString(title, (gpuGetViewportWidth() - gputGetStringWidth(title, 16)) / 2, (gpuGetViewportHeight() - gputGetStringHeight(title, 16) + gputGetStringHeight(str, 8)) / 2, 16, 16);
    gputDrawString(str, (gpuGetViewportWidth() - gputGetStringWidth(str, 8)) / 2, 4, 8, 8);

    return breakLoop;
}

int main(int argc, char **argv) {
    if(!platformInit(argc)) {
        return 0;
    }

    freeSpace = fsGetFreeSpace(destination);
    while(platformIsRunning()) {
        if(mode == INSTALL_CIA || mode == DELETE_CIA) {
            uiDisplayMessage(BOTTOM_SCREEN, "Loading file list...");

            std::string fileTarget;
            uiSelectFile(&fileTarget, "/", extensions, [&](const std::string currDirectory, bool inRoot, bool &updateList) {
                if(inputIsPressed(BUTTON_X)) {
                    std::stringstream confirmMsg;
                    if(mode == INSTALL_CIA) {
                        confirmMsg << "Install ";
                    } else {
                        confirmMsg << "Delete ";
                    }

                    confirmMsg << "all CIAs in the current directory?";
                    if(uiPrompt(TOP_SCREEN, confirmMsg.str(), true)) {
                        bool failed = false;
                        std::vector<FileInfo> contents = fsGetDirectoryContents(currDirectory);
                        for(std::vector<FileInfo>::iterator it = contents.begin(); it != contents.end(); it++) {
                            std::string path = (*it).path;
                            std::string name = (*it).name;
                            if(!fsIsDirectory(name) && fsHasExtensions(name, extensions)) {
                                if(mode == INSTALL_CIA) {
                                    if(!installCIA(destination, path, name)) {
                                        failed = true;
                                        break;
                                    }
                                } else {
                                    if(deleteCIA(path, name)) {
                                        updateList = true;
                                    } else {
                                        failed = true;
                                        break;
                                    }
                                }
                            }
                        }

                        if(!failed) {
                            std::stringstream successMsg;
                            if(mode == INSTALL_CIA) {
                                successMsg << "Install ";
                            } else {
                                successMsg << "Delete ";
                            }

                            successMsg << "succeeded!";
                            uiPrompt(TOP_SCREEN, successMsg.str(), false);
                        }

                        freeSpace = fsGetFreeSpace(destination);
                    }
                }

                return onLoop();
            }, [&](const std::string path, bool &updateList) {
                std::stringstream confirmMsg;
                if(mode == INSTALL_CIA) {
                    confirmMsg << "Install ";
                } else {
                    confirmMsg << "Delete ";
                }

                confirmMsg << "the selected CIA?";
                if(uiPrompt(TOP_SCREEN, confirmMsg.str(), true)) {
                    bool success = false;
                    if(mode == INSTALL_CIA) {
                        success = installCIA(destination, path, fsGetFileName(path));
                    } else {
                        success = deleteCIA(path, fsGetFileName(path));
                    }

                    if(success) {
                        std::stringstream successMsg;
                        if(mode == INSTALL_CIA) {
                            successMsg << "Install ";
                        } else {
                            successMsg << "Delete ";
                        }

                        successMsg << "succeeded!";
                        uiPrompt(TOP_SCREEN, successMsg.str(), false);
                    }

                    freeSpace = fsGetFreeSpace(destination);
                }

                return false;
            });
        } else if(mode == DELETE_TITLE || mode == LAUNCH_TITLE) {
            uiDisplayMessage(BOTTOM_SCREEN, "Loading title list...");

            App appTarget;
            uiSelectApp(&appTarget, destination, [&](bool &updateList) {
                return onLoop();
            }, [&](App app, bool &updateList) {
                if(mode == DELETE_TITLE) {
                    if(uiPrompt(TOP_SCREEN, "Delete the selected title?", true) && (destination != NAND || uiPrompt(TOP_SCREEN, "You are about to delete a title from the NAND.\nTHIS HAS THE POTENTIAL TO BRICK YOUR 3DS!\nAre you sure you wish to continue?", true))) {
                        if(deleteTitle(app)) {
                            uiPrompt(TOP_SCREEN, "Delete succeeded!", false);
                        }

                        updateList = true;
                        freeSpace = fsGetFreeSpace(destination);
                    }
                } else if(mode == LAUNCH_TITLE) {
                    if(uiPrompt(TOP_SCREEN, "Launch the selected title?", true)) {
                        if(launchTitle(app)) {
                            while(true) {
                            }
                        }
                    }
                }

                return false;
            });
        }

        if(exit) {
            break;
        }
    }

    platformCleanup();
    return 0;
}
