#include <ctrcommon/app.hpp>
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
    DELETE_TITLE,
    LAUNCH_TITLE
} Mode;

int main(int argc, char **argv) {
    if(!platformInit()) {
        return 0;
    }

    bool ninjhax = platformIsNinjhax();

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
            netInstall = true;
            breakLoop = true;
        }

        std::stringstream stream;
        stream << "FBI v1.3.6" << "\n";
        stream << "Free Space: " << freeSpace << " bytes (" << std::fixed << std::setprecision(2) << freeSpace / 1024.0f / 1024.0f << "MB)" << "\n";
        stream << "Destination: " << (destination == NAND ? "NAND" : "SD") << ", Mode: " << (mode == INSTALL_CIA ? "Install CIA" : mode == DELETE_CIA ? "Delete CIA" : mode == DELETE_TITLE ? "Delete Title" : "Launch Title") << "\n";
        stream << "L - Switch Destination, R - Switch Mode" << "\n";
        if(mode == INSTALL_CIA) {
            stream << "X - Install all CIAs in the current directory" << "\n";
            stream << "Y - Receive an app over the network" << "\n";
        } else if(mode == DELETE_CIA) {
            stream << "X - Delete all CIAs in the current directory" << "\n";
        }

        if(ninjhax) {
            stream << "START - Exit to launcher" << "\n";
        }

        std::string str = stream.str();
        screenDrawString(str, (screenGetWidth() - screenGetStrWidth(str)) / 2, screenGetHeight() - 4 - screenGetStrHeight(str), 255, 255, 255);

        return breakLoop;
    };

    std::string batchInfo = "";

    int prevProgress = -1;
    auto onProgress = [&](u64 pos, u64 totalSize) {
        u32 progress = (u32) ((pos * 100) / totalSize);
        if(prevProgress != (int) progress) {
            prevProgress = (int) progress;

            std::stringstream details;
            details << batchInfo;
            details << "Press B to cancel.";

            uiDisplayProgress(TOP_SCREEN, "Installing", details.str(), true, progress);
        }

        inputPoll();
        return !inputIsPressed(BUTTON_B);
    };

    bool showNetworkPrompts = true;

    while(platformIsRunning()) {
        std::string fileTarget;
        App appTarget;
        if(mode == INSTALL_CIA || mode == DELETE_CIA) {
            if(netInstall && !exit) {
                screenClearBuffers(BOTTOM_SCREEN, 0, 0, 0);

                RemoteFile file = uiAcceptRemoteFile(TOP_SCREEN, [&](std::stringstream& infoStream) {
                    if(inputIsPressed(BUTTON_A)) {
                        showNetworkPrompts = !showNetworkPrompts;
                    }

                    infoStream << "\n";
                    infoStream << "Prompts: " << (showNetworkPrompts ? "Enabled" : "Disabled") << "\n";
                    infoStream << "Press A to toggle prompts." << "\n";
                });

                if(file.fd == NULL) {
                    netInstall = false;
                    continue;
                }

                std::stringstream confirmStream;
                confirmStream << "Install the received application?" << "\n";
                confirmStream << "Size: " << file.fileSize << " bytes (" << std::fixed << std::setprecision(2) << file.fileSize / 1024.0f / 1024.0f << "MB)" << "\n";
                if(!showNetworkPrompts || uiPrompt(TOP_SCREEN, confirmStream.str(), true)) {
                    AppResult ret = appInstall(destination, file.fd, file.fileSize, onProgress);
                    prevProgress = -1;
                    if(showNetworkPrompts || ret != APP_SUCCESS) {
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
                }

                fclose(file.fd);
                continue;
            }

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
                        u32 currItem = 0;
                        for(std::vector<FileInfo>::iterator it = contents.begin(); it != contents.end(); it++) {
                            std::string path = (*it).path;
                            std::string fileName = (*it).name;
                            if(!fsIsDirectory(path) && fsHasExtensions(path, extensions)) {
                                if(mode == INSTALL_CIA) {
                                    std::stringstream batchInstallStream;
                                    batchInstallStream << fileName << " (" << currItem << ")" << "\n";

                                    batchInfo = batchInstallStream.str();
                                    AppResult ret = appInstallFile(destination, path, onProgress);
                                    prevProgress = -1;
                                    batchInfo = "";
                                    if(ret != APP_SUCCESS) {
                                        Error error = platformGetError();
                                        platformSetError(error);

                                        std::stringstream resultMsg;
                                        resultMsg << "Install failed!" << "\n";
                                        resultMsg << fileName << "\n";
                                        resultMsg << appGetResultString(ret) << "\n";
                                        uiPrompt(TOP_SCREEN, resultMsg.str(), false);
                                        if(error.module != MODULE_AM || error.description != DESCRIPTION_ALREADY_EXISTS) {
                                            failed = true;
                                            break;
                                        }
                                    }
                                } else {
                                    std::stringstream deleteStream;
                                    deleteStream << "Deleting CIA..." << "\n";
                                    deleteStream << fileName << " (" << currItem << ")" << "\n";

                                    uiDisplayMessage(TOP_SCREEN, deleteStream.str());
                                    if(!fsDelete(path)) {
                                        std::stringstream resultMsg;
                                        resultMsg << "Delete failed!" << "\n";
                                        resultMsg << fileName << "\n";
                                        resultMsg << platformGetErrorString(platformGetError()) << "\n";
                                        uiPrompt(TOP_SCREEN, resultMsg.str(), false);
                                        failed = true;
                                        break;
                                    } else {
                                        updateList = true;
                                    }
                                }
                            }

                            currItem++;
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
                        prevProgress = -1;
                        if(ret == APP_SUCCESS) {
                            resultMsg << "succeeded!";
                        } else {
                            resultMsg << "failed!" << "\n";
                            resultMsg << appGetResultString(ret) << "\n";
                        }
                    } else {
                        uiDisplayMessage(TOP_SCREEN, "Deleting CIA...");
                        if(fsDelete(path)) {
                            updateList = true;
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
        } else if(mode == DELETE_TITLE || mode == LAUNCH_TITLE) {
            uiDisplayMessage(BOTTOM_SCREEN, "Loading title list...");
            uiSelectApp(&appTarget, destination, [&](bool &updateList) {
                return onLoop();
            }, [&](App app, bool &updateList) {
                if(mode == DELETE_TITLE) {
                    if(uiPrompt(TOP_SCREEN, "Delete the selected title?", true) && (destination != NAND || uiPrompt(TOP_SCREEN, "You are about to delete a title from the NAND.\nTHIS HAS THE POTENTIAL TO BRICK YOUR 3DS!\nAre you sure you wish to continue?", true))) {
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
                } else if(mode == LAUNCH_TITLE) {
                    if(uiPrompt(TOP_SCREEN, "Launch the selected title?", true)) {
                        updateList = true;
                        uiDisplayMessage(TOP_SCREEN, "Launching title...");
                        AppResult ret = appLaunch(app);

                        if(ret != APP_SUCCESS) {
                            std::stringstream resultMsg;
                            resultMsg << "Launch failed!" << "\n";
                            resultMsg << appGetResultString(ret) << "\n";
                            uiPrompt(TOP_SCREEN, resultMsg.str(), false);
                        } else {
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
