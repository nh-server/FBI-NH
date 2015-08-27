#include <citrus/app.hpp>
#include <citrus/battery.hpp>
#include <citrus/core.hpp>
#include <citrus/err.hpp>
#include <citrus/fs.hpp>
#include <citrus/gpu.hpp>
#include <citrus/gput.hpp>
#include <citrus/hid.hpp>
#include <citrus/nor.hpp>
#include <citrus/wifi.hpp>

#include "rop.h"
#include "ui.hpp"

#include <sys/stat.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <iomanip>
#include <sstream>

using namespace ctr;

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
fs::MediaType destination = fs::SD;
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

        uiDisplayProgress(gpu::SCREEN_TOP, "Installing", details.str(), true, progress);
    }

    hid::poll();
    return !hid::pressed(hid::BUTTON_B);
}

void networkInstall() {
    while(core::running()) {
        RemoteFile file = uiAcceptRemoteFile(gpu::SCREEN_TOP, [&](std::stringstream& infoStream) {
            if(hid::pressed(hid::BUTTON_A)) {
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
        if(!showNetworkPrompts || uiPrompt(gpu::SCREEN_TOP, confirmStream.str(), true)) {
            app::install(destination, file.fd, file.fileSize, &onProgress);
            prevProgress = -1;
            if(showNetworkPrompts || err::has()) {
                std::stringstream resultMsg;
                resultMsg << "Install ";
                if(!err::has()) {
                    resultMsg << "succeeded!";
                } else {
                    err::Error error = err::get();
                    if(error.source == err::SOURCE_OPERATION_CANCELLED) {
                        resultMsg << "cancelled!";
                    } else {
                        resultMsg << "failed!" << "\n";
                        resultMsg << err::toString(error);
                    }
                }

                uiPrompt(gpu::SCREEN_TOP, resultMsg.str(), false);

                freeSpace = fs::freeSpace(destination);
            }
        }

        fclose(file.fd);
    }
}

void installROP() {
    u32 selected = 0;
    bool dirty = true;
    while(core::running()) {
        hid::poll();
        if(hid::pressed(hid::BUTTON_B)) {
            break;
        }

        if(hid::pressed(hid::BUTTON_A)) {
            std::stringstream stream;
            stream << "Install the selected ROP?" << "\n";
            stream << ropNames[selected];

            if(uiPrompt(gpu::SCREEN_TOP, stream.str(), true)) {
                u16 userSettingsOffset = 0;
                nor::read(0x20, &userSettingsOffset, 2);
                if(!err::has()) {
                    nor::write(userSettingsOffset << 3, rops[selected], ROP_SIZE);
                }

                std::stringstream resultMsg;
                resultMsg << "ROP installation ";
                if(!err::has()) {
                    resultMsg << "succeeded!";
                } else {
                    resultMsg << "failed!" << "\n";
                    resultMsg << err::toString(err::get());
                }

                uiPrompt(gpu::SCREEN_TOP, resultMsg.str(), false);
            }

            dirty = true;
        }

        if(hid::pressed(hid::BUTTON_LEFT)) {
            if(selected == 0) {
                selected = ROP_COUNT - 1;
            } else {
                selected--;
            }

            dirty = true;
        }

        if(hid::pressed(hid::BUTTON_RIGHT)) {
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
            uiDisplayMessage(gpu::SCREEN_TOP, stream.str());
        }
    }
}

bool installCIA(fs::MediaType destination, const std::string path, int curr, int total) {
    std::string name = fs::fileName(path);
    if(name.length() > 40) {
        name.resize(40);
        name += "...";
    }

    FILE* fd = fopen(path.c_str(), "r");
    if(!fd) {
        std::stringstream resultMsg;
        resultMsg << "Install failed!" << "\n";
        resultMsg << name << "\n";
        resultMsg << "Could not open file." << "\n";
        resultMsg << strerror(errno) << "\n";
        uiPrompt(gpu::SCREEN_TOP, resultMsg.str(), false);

        return false;
    }

    struct stat st;
    fstat(fileno(fd), &st);

    std::stringstream batchInstallStream;
    batchInstallStream << name << " (" << curr << "/" << total << ")" << "\n";

    installInfo = batchInstallStream.str();
    app::install(destination, fd, (u64) st.st_size, &onProgress);
    prevProgress = -1;
    installInfo = "";

    if(err::has()) {
        err::Error error = err::get();
        if(error.module == err::MODULE_NN_AM && error.description == err::DESCRIPTION_ALREADY_EXISTS) {
            std::stringstream overwriteMsg;
            overwriteMsg << "Title already installed, overwrite?" << "\n";
            overwriteMsg << name;
            if(uiPrompt(gpu::SCREEN_TOP, overwriteMsg.str(), true)) {
                uiDisplayMessage(gpu::SCREEN_TOP, "Deleting title...");

                app::uninstall(app::ciaInfo(path, destination));
                if(err::has()) {
                    std::stringstream resultMsg;
                    resultMsg << "Delete failed!" << "\n";
                    resultMsg << name << "\n";
                    resultMsg << err::toString(err::get());
                    uiPrompt(gpu::SCREEN_TOP, resultMsg.str(), false);

                    return false;
                }

                installInfo = batchInstallStream.str();
                app::install(destination, fd, (u64) st.st_size, &onProgress);
                prevProgress = -1;
                installInfo = "";
            } else {
                err::set(error);
            }
        } else {
            err::set(error);
        }
    }

    if(err::has()) {
        err::Error error = err::get();
        std::stringstream resultMsg;
        resultMsg << "Install ";
        if(error.source == err::SOURCE_OPERATION_CANCELLED) {
            resultMsg << "cancelled!" << "\n";
            resultMsg << name;
        } else {
            resultMsg << "failed!" << "\n";
            resultMsg << name << "\n";
            resultMsg << err::toString(error);
        }

        uiPrompt(gpu::SCREEN_TOP, resultMsg.str(), false);

        return false;
    }

    return true;
}

bool deleteCIA(const std::string path, int curr, int total) {
    std::string name = fs::fileName(path);
    if(name.length() > 40) {
        name.resize(40);
        name += "...";
    }

    std::stringstream deleteStream;
    deleteStream << "Deleting CIA..." << "\n";
    deleteStream << name << " (" << curr << "/" << total << ")";
    uiDisplayMessage(gpu::SCREEN_TOP, deleteStream.str());

    if(remove(path.c_str()) != 0) {
        std::stringstream resultMsg;
        resultMsg << "Delete failed!" << "\n";
        resultMsg << name << "\n";
        resultMsg << strerror(errno);
        uiPrompt(gpu::SCREEN_TOP, resultMsg.str(), false);

        return false;
    }

    return true;
}

bool deleteTitle(app::App app) {
    uiDisplayMessage(gpu::SCREEN_TOP, "Deleting title...");

    app::uninstall(app);
    if(err::has()) {
        std::stringstream resultMsg;
        resultMsg << "Delete failed!" << "\n";
        resultMsg << err::toString(err::get());
        uiPrompt(gpu::SCREEN_TOP, resultMsg.str(), false);

        return false;
    }

    return true;
}

bool launchTitle(app::App app) {
    uiDisplayMessage(gpu::SCREEN_TOP, "Launching title...");

    app::launch(app);
    if(err::has()) {
        std::stringstream resultMsg;
        resultMsg << "Launch failed!" << "\n";
        resultMsg << err::toString(err::get());
        uiPrompt(gpu::SCREEN_TOP, resultMsg.str(), false);

        return false;
    }

    return true;
}

bool onLoop() {
    bool launcher = core::launcher();
    if(launcher && hid::pressed(hid::BUTTON_START)) {
        exit = true;
        return true;
    }

    bool breakLoop = false;

    if(hid::pressed(hid::BUTTON_L)) {
        if(destination == fs::SD) {
            destination = fs::NAND;
        } else {
            destination = fs::SD;
        }

        freeSpace = fs::freeSpace(destination);
        if(mode == DELETE_TITLE || mode == LAUNCH_TITLE) {
            breakLoop = true;
        }
    }

    if(hid::pressed(hid::BUTTON_R)) {
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

    if(mode == INSTALL_CIA && hid::pressed(hid::BUTTON_Y)) {
        networkInstall();
    }

    if(hid::pressed(hid::BUTTON_SELECT)) {
        installROP();
    }

    std::stringstream stream;
    stream << "Battery: ";
    if(battery::charging()) {
        stream << "Charging";
    } else {
        for(u32 i = 0; i < battery::level(); i++) {
            stream << "|";
        }
    }

    stream << ", WiFi: ";
    if(!wifi::connected()) {
        stream << "Disconnected";
    } else {
        for(u32 i = 0; i < wifi::strength(); i++) {
            stream << "|";
        }
    }

    stream << "\n";

    stream << "Free Space: " << freeSpace << " bytes (" << std::fixed << std::setprecision(2) << freeSpace / 1024.0f / 1024.0f << "MB)" << "\n";
    stream << "Destination: " << (destination == fs::NAND ? "NAND" : "SD") << ", Mode: " << (mode == INSTALL_CIA ? "Install CIA" : mode == DELETE_CIA ? "Delete CIA" : mode == DELETE_TITLE ? "Delete Title" : "Launch Title") << "\n";
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

    u32 screenWidth;
    u32 screenHeight;
    gpu::getViewportWidth(&screenWidth);
    gpu::getViewportHeight(&screenHeight);

    std::string str = stream.str();
    const std::string title = "FBI v1.4.10";
    gput::drawString(title, (screenWidth - gput::getStringWidth(title, 16)) / 2, (screenHeight - gput::getStringHeight(title, 16) + gput::getStringHeight(str, 8)) / 2, 16, 16);
    gput::drawString(str, (screenWidth - gput::getStringWidth(str, 8)) / 2, 4, 8, 8);

    return breakLoop;
}

int main(int argc, char **argv) {
    if(!core::init(argc)) {
        return 0;
    }

    uiInit();

    freeSpace = fs::freeSpace(destination);
    while(core::running()) {
        if(mode == INSTALL_CIA || mode == DELETE_CIA) {
            uiDisplayMessage(gpu::SCREEN_BOTTOM, "Loading file list...");

            std::string fileTarget;
            uiSelectFile(&fileTarget, "/", extensions, [&](const std::string currDirectory, bool inRoot, bool &updateList) {
                if(hid::pressed(hid::BUTTON_X)) {
                    std::stringstream confirmMsg;
                    if(mode == INSTALL_CIA) {
                        confirmMsg << "Install ";
                    } else {
                        confirmMsg << "Delete ";
                    }

                    confirmMsg << "all CIAs in the current directory?";
                    if(uiPrompt(gpu::SCREEN_TOP, confirmMsg.str(), true)) {
                        bool failed = false;
                        std::vector<std::string> contents = fs::contents(currDirectory, false, extensions);
                        int total = contents.size();
                        int curr = 1;
                        for(std::vector<std::string>::iterator it = contents.begin(); it != contents.end(); it++) {
                            std::string path = *it;
                            if(mode == INSTALL_CIA) {
                                if(!installCIA(destination, path, curr, total)) {
                                    failed = true;
                                    break;
                                }
                            } else {
                                if(deleteCIA(path, curr, total)) {
                                    updateList = true;
                                } else {
                                    failed = true;
                                    break;
                                }
                            }

                            curr++;
                        }

                        if(!failed) {
                            std::stringstream successMsg;
                            if(mode == INSTALL_CIA) {
                                successMsg << "Install ";
                            } else {
                                successMsg << "Delete ";
                            }

                            successMsg << "succeeded!";
                            uiPrompt(gpu::SCREEN_TOP, successMsg.str(), false);
                        }

                        freeSpace = fs::freeSpace(destination);
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
                if(uiPrompt(gpu::SCREEN_TOP, confirmMsg.str(), true)) {
                    bool success = false;
                    if(mode == INSTALL_CIA) {
                        success = installCIA(destination, path, 1, 1);
                    } else {
                        success = deleteCIA(path, 1, 1);
                    }

                    if(success) {
                        std::stringstream successMsg;
                        if(mode == INSTALL_CIA) {
                            successMsg << "Install ";
                        } else {
                            successMsg << "Delete ";
                        }

                        successMsg << "succeeded!";
                        uiPrompt(gpu::SCREEN_TOP, successMsg.str(), false);
                    }

                    freeSpace = fs::freeSpace(destination);
                }

                return false;
            });
        } else if(mode == DELETE_TITLE || mode == LAUNCH_TITLE) {
            uiDisplayMessage(gpu::SCREEN_BOTTOM, "Loading title list...");

            app::App appTarget;
            uiSelectApp(&appTarget, destination, [&](bool &updateList) {
                return onLoop();
            }, [&](app::App app, bool &updateList) {
                if(mode == DELETE_TITLE) {
                    if(uiPrompt(gpu::SCREEN_TOP, "Delete the selected title?", true) && (destination != fs::NAND || uiPrompt(gpu::SCREEN_TOP, "You are about to delete a title from the NAND.\nTHIS HAS THE POTENTIAL TO BRICK YOUR 3DS!\nAre you sure you wish to continue?", true))) {
                        if(deleteTitle(app)) {
                            uiPrompt(gpu::SCREEN_TOP, "Delete succeeded!", false);
                        }

                        updateList = true;
                        freeSpace = fs::freeSpace(destination);
                    }
                } else if(mode == LAUNCH_TITLE) {
                    if(uiPrompt(gpu::SCREEN_TOP, "Launch the selected title?", true)) {
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

    uiCleanup();

    core::exit();
    return 0;
}
