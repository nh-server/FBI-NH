#include <ctrcommon/input.hpp>
#include <ctrcommon/platform.hpp>
#include <ctrcommon/ui.hpp>

#include <stdio.h>

#include <sstream>
#include <iomanip>

#include <sys/dirent.h>

typedef enum {
    INSTALL_CIA,
    DELETE_CIA,
    DELETE_TITLE
} Mode;

int main(int argc, char **argv) {
    if(!platformInit()) {
        return 0;
    }

    bool ninjhax = platformIsNinjhax();
    /* if(ninjhax) {
        consoleInit(GFX_BOTTOM, NULL);
        consoleClear();

        AcquireResult result = platformAcquireServices();
        if(result != ACQUIRE_SUCCESS) {
            //std::stringstream errorStream;
            //errorStream << "Failed to acquire services." << "\n";
            //errorStream << platformGetAcquireResultString(result) << "\n";
            //uiPrompt(TOP_SCREEN, errorStream.str(), false);

            while(true) {
                inputPoll();
                if(inputIsPressed(BUTTON_START)) {
                    break;
                }
            }

            platformCleanup();
            return 0;
        }
    } */

    std::vector<std::string> extensions;
    extensions.push_back("cia");

    MediaType destination = SD;
    Mode mode = INSTALL_CIA;
    bool exit = false;
    bool netInstall = false;
    u64 freeSpace = fsGetFreeSpace(destination);
    auto onLoop = [&]() {
        if(ninjhax && inputIsPressed(BUTTON_START)) {
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
            if(mode == DELETE_TITLE) {
                breakLoop = true;
            }
        }

        if(inputIsPressed(BUTTON_R)) {
            if(mode == INSTALL_CIA) {
                mode = DELETE_CIA;
            } else if(mode == DELETE_CIA) {
                mode = DELETE_TITLE;
                breakLoop = true;
            } else {
                mode = INSTALL_CIA;
                breakLoop = true;
            }
        }

        if(inputIsPressed(BUTTON_Y)) {
            netInstall = true;
            breakLoop = true;
        }

        std::stringstream stream;
        stream << "Free Space: " << freeSpace << " bytes (" << std::fixed << std::setprecision(2) << freeSpace / 1024.0f / 1024.0f << "MB)" << "\n";
        stream << "Destination: " << (destination == NAND ? "NAND" : "SD") << ", Mode: " << (mode == INSTALL_CIA ? "Install CIA" : mode == DELETE_CIA ? "Delete CIA" : "Delete Title") << "\n";
        stream << "L - Switch Destination, R - Switch Mode" << "\n";
        if(mode == INSTALL_CIA) {
            stream << "X - Install all CIAs in the current directory" << "\n";
        } else if(mode == DELETE_CIA) {
            stream << "X - Delete all CIAs in the current directory" << "\n";
        }

        stream << "Y - Receive an app over the network" << "\n";
        if(ninjhax) {
            stream << "START - Exit to launcher" << "\n";
        }

        std::string str = stream.str();
        screenDrawString(str, (screenGetWidth() - screenGetStrWidth(str)) / 2, screenGetHeight() - 4 - screenGetStrHeight(str), 255, 255, 255);

        return breakLoop;
    };

    auto onProgress = [&](int progress) {
        uiDisplayProgress(TOP_SCREEN, "Installing", "Press B to cancel.", true, progress);
        inputPoll();
        return !inputIsPressed(BUTTON_B);
    };

    while(platformIsRunning()) {
        std::string fileTarget;
        App appTarget;
        if(mode == INSTALL_CIA || mode == DELETE_CIA) {
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
                            std::string fileName = (*it).name;
                            if(!fsIsDirectory(path) && fsHasExtensions(path, extensions)) {
                                if(mode == INSTALL_CIA) {
                                    AppResult ret = appInstallFile(destination, path, onProgress);
                                    if(ret != APP_SUCCESS) {
                                        std::stringstream resultMsg;
                                        resultMsg << "Install failed!" << "\n";
                                        resultMsg << fileName << "\n";
                                        resultMsg << appGetResultString(ret) << "\n";
                                        uiPrompt(TOP_SCREEN, resultMsg.str(), false);
                                        failed = true;
                                        break;
                                    }
                                } else {
                                    if(!fsDelete(path)) {
                                        std::stringstream resultMsg;
                                        resultMsg << "Delete failed!" << "\n";
                                        resultMsg << fileName << "\n";
                                        resultMsg << platformGetErrorString(platformGetError()) << "\n";
                                        uiPrompt(TOP_SCREEN, resultMsg.str(), false);
                                        failed = true;
                                        break;
                                    }
                                }
                            }
                        }

                        if(!failed) {
                            uiPrompt(TOP_SCREEN, "Install succeeded!\n", false);
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
                    std::stringstream resultMsg;
                    if(mode == INSTALL_CIA) {
                        resultMsg << "Install ";
                    } else {
                        resultMsg << "Delete ";
                    }

                    if(mode == INSTALL_CIA) {
                        AppResult ret = appInstallFile(destination, path, onProgress);
                        if(ret == APP_SUCCESS) {
                            resultMsg << "succeeded!";
                        } else {
                            resultMsg << "failed!" << "\n";
                            resultMsg << appGetResultString(ret) << "\n";
                        }
                    } else {
                        if(fsDelete(path)) {
                            resultMsg << "succeeded!";
                        } else {
                            resultMsg << "failed!" << "\n";
                            resultMsg << platformGetErrorString(platformGetError()) << "\n";
                        }
                    }

                    uiPrompt(TOP_SCREEN, resultMsg.str(), false);

                    freeSpace = fsGetFreeSpace(destination);
                }

                return false;
            });
        } else if(mode == DELETE_TITLE) {
            uiSelectApp(&appTarget, destination, [&](bool &updateList) {
                return onLoop();
            }, [&](App app, bool &updateList) {
                if(uiPrompt(TOP_SCREEN, "Delete the selected title?", true)) {
                    updateList = true;
                    uiDisplayMessage(TOP_SCREEN, "Deleting title...");
                    AppResult ret = appDelete(app);

                    std::stringstream resultMsg;
                    resultMsg << "Delete ";
                    if(ret == APP_SUCCESS) {
                        resultMsg << "succeeded!";
                    } else {
                        resultMsg << "failed!" << "\n";
                        resultMsg << appGetResultString(ret) << "\n";
                    }

                    uiPrompt(TOP_SCREEN, resultMsg.str(), false);

                    freeSpace = fsGetFreeSpace(destination);
                }

                return false;
            });
        }

        if(netInstall && !exit) {
            netInstall = false;

            screenClearBuffers(BOTTOM_SCREEN, 0, 0, 0);

            RemoteFile file = uiAcceptRemoteFile(TOP_SCREEN);
            if(file.fd == NULL) {
                continue;
            }

            std::stringstream confirmStream;
            confirmStream << "Install the received application?" << "\n";
            confirmStream << "Size: " << file.fileSize << " bytes (" << std::fixed << std::setprecision(2) << file.fileSize / 1024.0f / 1024.0f << "MB)" << "\n";
            if(uiPrompt(TOP_SCREEN, confirmStream.str(), true)) {
                AppResult ret = appInstall(destination, file.fd, file.fileSize, onProgress);
                std::stringstream resultMsg;
                resultMsg << "Install ";
                if(ret == APP_SUCCESS) {
                    resultMsg << "succeeded!";
                } else {
                    resultMsg << "failed!" << "\n";
                    resultMsg << appGetResultString(ret) << "\n";
                }

                uiPrompt(TOP_SCREEN, resultMsg.str(), false);
            }

            fclose(file.fd);
            continue;
        }

        if(exit) {
            break;
        }
    }

    platformCleanup();
    return 0;
}
