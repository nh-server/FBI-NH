#include "ui.hpp"

#include <citrus/core.hpp>
#include <citrus/err.hpp>
#include <citrus/gput.hpp>
#include <citrus/hid.hpp>

#include <arpa/inet.h>
#include <sys/dirent.h>
#include <sys/errno.h>
#include <sys/unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <stack>

using namespace ctr;

struct uiAlphabetize {
    inline bool operator()(SelectableElement a, SelectableElement b) {
        return strcasecmp(a.name.c_str(), b.name.c_str()) < 0;
    }
};

u32 selectorTexture;
u32 selectorVbo;

void uiInit() {
    gpu::createTexture(&selectorTexture);
    gpu::setTextureInfo(selectorTexture, 64, 64, gpu::PIXEL_RGBA8, gpu::textureMinFilter(gpu::FILTER_NEAREST) | gpu::textureMagFilter(gpu::FILTER_NEAREST));

    void* textureData;
    gpu::getTextureData(selectorTexture, &textureData);
    memset(textureData, 0xFF, 64 * 64 * 4);

    gpu::createVbo(&selectorVbo);
    gpu::setVboAttributes(selectorVbo, gpu::vboAttribute(0, 3, gpu::ATTR_FLOAT) | gpu::vboAttribute(1, 2, gpu::ATTR_FLOAT) | gpu::vboAttribute(2, 4, gpu::ATTR_FLOAT), 3);

    const float vboData[] = {
            0.0f, 0.0f, -0.1f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f,
            320.0f, 0.0f, -0.1f, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f,
            320.0f, 12.0f, -0.1f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
            320.0f, 12.0f, -0.1f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
            0.0f, 12.0f, -0.1f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
            0.0f, 0.0f, -0.1f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f,
    };

    gpu::setVboData(selectorVbo, vboData, 6 * 9, gpu::PRIM_TRIANGLES);
}

void uiCleanup() {
    if(selectorTexture != 0) {
        gpu::freeTexture(selectorTexture);
        selectorTexture = 0;
    }

    if(selectorVbo != 0) {
        gpu::freeVbo(selectorVbo);
        selectorVbo = 0;
    }
}

bool uiSelect(SelectableElement* selected, std::vector<SelectableElement> elements, std::function<bool(std::vector<SelectableElement> &currElements, SelectableElement currElement, bool &elementsDirty, bool &resetCursorIfDirty)> onLoop, std::function<bool(SelectableElement select)> onSelect, bool useTopScreen, bool alphabetize, bool dpadPageScroll) {
    int cursor = 0;
    int scroll = 0;

    u32 selectionScroll = 0;
    u64 selectionScrollEndTime = 0;

    u64 lastScrollTime = 0;

    bool elementsDirty = false;
    bool resetCursorIfDirty = true;
    if(alphabetize) {
        std::sort(elements.begin(), elements.end(), uiAlphabetize());
    }

    hid::Button leftScrollButton = dpadPageScroll ? hid::BUTTON_LEFT : hid::BUTTON_L;
    hid::Button rightScrollButton = dpadPageScroll ? hid::BUTTON_RIGHT : hid::BUTTON_R;

    bool canPageUp = false;
    bool canPageDown = false;
    while(core::running()) {
        hid::poll();
        if(hid::pressed(hid::BUTTON_A)) {
            SelectableElement select = elements.at((u32) cursor);
            if(onSelect == NULL || onSelect(select)) {
                *selected = select;
                return true;
            }
        }

        if(canPageUp) {
            canPageUp = !hid::released(leftScrollButton);
        } else if(hid::pressed(leftScrollButton)) {
            canPageUp = true;
        }

        if(canPageDown) {
            canPageDown = !hid::released(rightScrollButton);
        } else if(hid::pressed(rightScrollButton)) {
            canPageDown = true;
        }

        if(hid::held(hid::BUTTON_DOWN) || hid::held(hid::BUTTON_UP) || (hid::held(leftScrollButton) && canPageUp) || (hid::held(rightScrollButton) && canPageDown)) {
            if(lastScrollTime == 0 || core::time() - lastScrollTime >= 180) {
                if(hid::held(hid::BUTTON_DOWN) && cursor < (int) elements.size() - 1) {
                    cursor++;
                    if(cursor >= scroll + 20) {
                        scroll++;
                    }
                }

                if(canPageDown && hid::held(rightScrollButton) && cursor < (int) elements.size() - 1) {
                    cursor += 20;
                    if(cursor >= (int) elements.size()) {
                        cursor = elements.size() - 1;
                        if(cursor < 0) {
                            cursor = 0;
                        }
                    }

                    scroll += 20;
                    if(scroll >= (int) elements.size() - 19) {
                        scroll = elements.size() - 20;
                        if(scroll < 0) {
                            scroll = 0;
                        }
                    }
                }

                if(hid::held(hid::BUTTON_UP) && cursor > 0) {
                    cursor--;
                    if(cursor < scroll) {
                        scroll--;
                    }
                }

                if(canPageUp && hid::held(leftScrollButton) && cursor > 0) {
                    cursor -= 20;
                    if(cursor < 0) {
                        cursor = 0;
                    }

                    scroll -= 20;
                    if(scroll < 0) {
                        scroll = 0;
                    }
                }

                selectionScroll = 0;
                selectionScrollEndTime = 0;

                lastScrollTime = core::time();
            }
        } else {
            lastScrollTime = 0;
        }

        gpu::setViewport(gpu::SCREEN_BOTTOM, 0, 0, gpu::BOTTOM_WIDTH, gpu::BOTTOM_HEIGHT);
        gput::setOrtho(0, gpu::BOTTOM_WIDTH, 0, gpu::BOTTOM_HEIGHT, -1, 1);
        gpu::clear();

        u32 screenWidth;
        u32 screenHeight;
        gpu::getViewportWidth(&screenWidth);
        gpu::getViewportHeight(&screenHeight);

        for(std::vector<SelectableElement>::iterator it = elements.begin() + scroll; it != elements.begin() + scroll + 20 && it != elements.end(); it++) {
            SelectableElement element = *it;
            int index = it - elements.begin();
            u8 color = 255;
            int offset = 0;
            float itemHeight = gput::getStringHeight(element.name, 8) + 4;
            if(index == cursor) {
                color = 0;

                gput::pushModelView();
                gput::translate(0, (screenHeight - 1) - ((index - scroll + 1) * itemHeight), 0);
                gpu::bindTexture(gpu::TEXUNIT0, selectorTexture);
                gpu::drawVbo(selectorVbo);
                gput::popModelView();

                u32 width = (u32) gput::getStringWidth(element.name, 8);
                if(width > screenWidth) {
                    if(selectionScroll + screenWidth >= width) {
                        if(selectionScrollEndTime == 0) {
                            selectionScrollEndTime = core::time();
                        } else if(core::time() - selectionScrollEndTime >= 4000) {
                            selectionScroll = 0;
                            selectionScrollEndTime = 0;
                        }
                    } else {
                        selectionScroll++;
                    }
                }

                offset = -selectionScroll;
            }

            gput::drawString(element.name, offset, (screenHeight - 1) - ((index - scroll + 1) * itemHeight) + 2, 8, 8, color, color, color);
        }

        gpu::flushCommands();
        gpu::flushBuffer();

        gpu::setViewport(gpu::SCREEN_TOP, 0, 0, gpu::TOP_WIDTH, gpu::TOP_HEIGHT);
        gput::setOrtho(0, gpu::TOP_WIDTH, 0, gpu::TOP_HEIGHT, -1, 1);
        if(useTopScreen) {
            gpu::clear();

            SelectableElement currSelected = elements.at((u32) cursor);
            if(currSelected.details.size() != 0) {
                std::stringstream details;
                for(std::vector<std::string>::iterator it = currSelected.details.begin(); it != currSelected.details.end(); it++) {
                    details << *it << "\n";
                }

                gput::drawString(details.str(), 0, screenHeight - 1 - gput::getStringHeight(details.str(), 8), 8, 8);
            }
        }

        bool result = onLoop != NULL && onLoop(elements, elements.at((u32) cursor), elementsDirty, resetCursorIfDirty);
        if(elementsDirty) {
            if(resetCursorIfDirty) {
                cursor = 0;
                scroll = 0;
            } else if(cursor >= (int) elements.size()) {
                cursor = elements.size() - 1;
                if(cursor < 0) {
                    cursor = 0;
                }

                scroll = elements.size() - 20;
                if(scroll < 0) {
                    scroll = 0;
                }
            }

            selectionScroll = 0;
            selectionScrollEndTime = 0;
            if(alphabetize) {
                std::sort(elements.begin(), elements.end(), uiAlphabetize());
            }

            elementsDirty = false;
            resetCursorIfDirty = true;
        }

        if(useTopScreen) {
            gpu::flushCommands();
            gpu::flushBuffer();
        }

        gpu::swapBuffers(true);
        if(result) {
            break;
        }
    }

    return false;
}

void uiGetDirContents(std::vector<SelectableElement> &elements, const std::string directory, std::vector<std::string> extensions) {
    elements.clear();
    elements.push_back({".", "."});
    elements.push_back({"..", ".."});

    std::vector<std::string> contents = fs::contents(directory);
    for(std::vector<std::string>::iterator it = contents.begin(); it != contents.end(); it++) {
        const std::string path = *it;
        const std::string name = fs::fileName(path);
        if(fs::directory(path)) {
            elements.push_back({path, name});
        } else if(fs::hasExtensions(path, extensions)) {
            struct stat st;
            stat(path.c_str(), &st);

            std::vector<std::string> info;
            std::stringstream stream;
            stream << "File Size: " << ((u32) st.st_size) << " bytes (" << std::fixed << std::setprecision(2) << ((u32) st.st_size) / 1024.0f / 1024.0f << "MB)";
            info.push_back(stream.str());

            std::string extension = fs::extension(path);
            if(extension.compare("cia") == 0) {
                app::App app = app::ciaInfo(path, fs::SD);
                if(!err::has()) {
                    std::stringstream titleId;
                    titleId << "0x" << std::setfill('0') << std::setw(16) << std::hex << app.titleId;

                    std::stringstream uniqueId;
                    uniqueId << "0x" << std::setfill('0') << std::setw(8) << std::hex << app.uniqueId;

                    std::stringstream version;
                    version << "0x" << std::setfill('0') << std::hex << app.version;

                    std::stringstream size;
                    size << "" << app.size << " bytes (" << std::fixed << std::setprecision(2) << app.size / 1024.0f / 1024.0f << "MB)";

                    info.push_back("Installed Size: " + size.str());
                    info.push_back("Title ID: " + titleId.str());
                    info.push_back("Unique ID: " + uniqueId.str());
                    info.push_back("Product Code: " + std::string(app.productCode));
                    info.push_back("Platform: " + app::platformString(app.platform));
                    info.push_back("Category: " + app::categoryString(app.category));
                    info.push_back("Version: " + version.str());
                }
            }

            elements.push_back({path, name, info});
        }
    }
}

bool uiSelectFile(std::string* selectedFile, const std::string rootDirectory, std::vector<std::string> extensions, std::function<bool(const std::string currDirectory, bool inRoot, bool &updateList)> onLoop, std::function<bool(const std::string path, bool &updateList)> onSelect, bool useTopScreen, bool dpadPageScroll) {
    std::stack<std::string> directoryStack;
    std::string currDirectory = rootDirectory;

    std::vector<SelectableElement> elements;
    uiGetDirContents(elements, currDirectory, extensions);

    bool updateContents = false;
    bool resetCursor = true;
    SelectableElement selected;
    bool result = uiSelect(&selected, elements, [&](std::vector<SelectableElement> &currElements, SelectableElement currElement, bool &elementsDirty, bool &resetCursorIfDirty) {
        if(onLoop != NULL && onLoop(currDirectory, directoryStack.empty(), updateContents)) {
            return true;
        }

        if(hid::pressed(hid::BUTTON_B) && !directoryStack.empty()) {
            currDirectory = directoryStack.top();
            directoryStack.pop();
            updateContents = true;
        }

        if(updateContents) {
            uiGetDirContents(currElements, currDirectory, extensions);
            elementsDirty = true;
            resetCursorIfDirty = resetCursor;
            updateContents = false;
            resetCursor = true;
        }

        return false;
    }, [&](SelectableElement select) {
        if(select.name.compare(".") == 0) {
            return false;
        } else if(select.name.compare("..") == 0) {
            if(!directoryStack.empty()) {
                currDirectory = directoryStack.top();
                directoryStack.pop();
                updateContents = true;
            }

            return false;
        } else if(fs::directory(select.id)) {
            directoryStack.push(currDirectory);
            currDirectory = select.id;
            updateContents = true;
            return false;
        }

        bool updateList = false;
        bool ret = onSelect(select.id, updateList);
        if(updateList) {
            updateContents = true;
            resetCursor = false;
        }

        return ret;
    }, useTopScreen, true, dpadPageScroll);

    if(result) {
        *selectedFile = selected.id;
    }

    return result;
}

void uiGetApps(std::vector<SelectableElement> &elements, std::vector<app::App> apps) {
    elements.clear();
    for(std::vector<app::App>::iterator it = apps.begin(); it != apps.end(); it++) {
        app::App app = *it;

        std::stringstream titleId;
        titleId << "0x" << std::setfill('0') << std::setw(16) << std::hex << app.titleId;

        std::stringstream uniqueId;
        uniqueId << "0x" << std::setfill('0') << std::setw(8) << std::hex << app.uniqueId;

        std::stringstream version;
        version << "0x" << std::setfill('0') << std::hex << app.version;

        std::stringstream size;
        size << "" << app.size << " bytes (" << std::fixed << std::setprecision(2) << app.size / 1024.0f / 1024.0f << "MB)";

        std::vector<std::string> details;
        details.push_back("Title ID: " + titleId.str());
        details.push_back("Unique ID: " + uniqueId.str());
        details.push_back("Product Code: " + std::string(app.productCode));
        details.push_back("Platform: " + app::platformString(app.platform));
        details.push_back("Category: " + app::categoryString(app.category));
        details.push_back("Version: " + version.str());
        details.push_back("Size: " + size.str());

        elements.push_back({titleId.str(), app.productCode, details});
    }

    if(elements.size() == 0) {
        elements.push_back({"None", "None"});
    }
}

bool uiFindApp(app::App* result, std::string id, std::vector<app::App> apps) {
    for(std::vector<app::App>::iterator it = apps.begin(); it != apps.end(); it++) {
        app::App app = *it;
        if(app.titleId == (u64) strtoll(id.c_str(), NULL, 16)) {
            *result = app;
            return true;
        }
    }

    return false;
}

bool uiSelectApp(app::App* selectedApp, fs::MediaType mediaType, std::function<bool(bool &updateList)> onLoop, std::function<bool(app::App app, bool &updateList)> onSelect, bool useTopScreen, bool dpadPageScroll) {
    std::vector<SelectableElement> elements;

    std::vector<app::App> apps = app::list(mediaType);
    uiGetApps(elements, apps);

    bool updateContents = false;
    SelectableElement selected;
    bool result = uiSelect(&selected, elements, [&](std::vector<SelectableElement> &currElements, SelectableElement currElement, bool &elementsDirty, bool &resetCursorIfDirty) {
        if(onLoop != NULL && onLoop(updateContents)) {
            return true;
        }

        if(updateContents) {
            apps = app::list(mediaType);
            uiGetApps(currElements, apps);
            elementsDirty = true;
            resetCursorIfDirty = false;
            updateContents = false;
        }

        return false;
    }, [&](SelectableElement select) {
        if(select.name.compare("None") != 0) {
            app::App app;
            if(uiFindApp(&app, select.id, apps)) {
                bool updateList = false;
                bool ret = onSelect(app, updateList);
                if(updateList) {
                    updateContents = true;
                }

                return ret;
            }
        }

        return false;
    }, useTopScreen, true, dpadPageScroll);

    if(result) {
        app::App app;
        if(uiFindApp(&app, selected.id, apps)) {
            *selectedApp = app;
        }
    }

    return result;
}

void uiDisplayMessage(gpu::Screen screen, const std::string message) {
    u32 width = screen == gpu::SCREEN_TOP ? gpu::TOP_WIDTH : gpu::BOTTOM_WIDTH;
    u32 height = screen == gpu::SCREEN_TOP ? gpu::TOP_HEIGHT : gpu::BOTTOM_HEIGHT;

    gpu::setViewport(screen, 0, 0, width, height);
    gput::setOrtho(0, width, 0, height, -1, 1);

    gpu::clear();
    gput::drawString(message, (width - gput::getStringWidth(message, 8)) / 2, (height - gput::getStringHeight(message, 8)) / 2, 8, 8);
    gpu::flushCommands();
    gpu::flushBuffer();
    gpu::swapBuffers(true);

    gpu::setViewport(gpu::SCREEN_TOP, 0, 0, gpu::TOP_WIDTH, gpu::TOP_HEIGHT);
    gput::setOrtho(0, gpu::TOP_WIDTH, 0, gpu::TOP_HEIGHT, -1, 1);
}

bool uiPrompt(gpu::Screen screen, const std::string message, bool question) {
    std::stringstream stream;
    stream << message << "\n";
    if(question) {
        stream << "Press A to confirm, B to cancel." << "\n";
    } else {
        stream << "Press Start to continue." << "\n";
    }

    bool result = false;
    std::string str = stream.str();
    while(core::running()) {
        hid::poll();
        if(question) {
            if(hid::pressed(hid::BUTTON_A)) {
                result = true;
                break;
            }

            if(hid::pressed(hid::BUTTON_B)) {
                result = false;
                break;
            }
        } else {
            if(hid::pressed(hid::BUTTON_START)) {
                result = true;
                break;
            }
        }

        uiDisplayMessage(screen, str);
    }

    hid::poll();
    return result;
}

void uiDisplayProgress(gpu::Screen screen, const std::string operation, const std::string details, bool quickSwap, u32 progress) {
    std::stringstream stream;
    stream << operation << ": [";
    u32 progressBars = progress / 4;
    for(u32 i = 0; i < 25; i++) {
        if(i < progressBars) {
            stream << '|';
        } else {
            stream << ' ';
        }
    }

    stream << "] " << std::setfill(' ') << std::setw(3) << progress << "%" << "\n";
    stream << details << "\n";

    std::string str = stream.str();

    u32 width = screen == gpu::SCREEN_TOP ? gpu::TOP_WIDTH : gpu::BOTTOM_WIDTH;
    u32 height = screen == gpu::SCREEN_TOP ? gpu::TOP_HEIGHT : gpu::BOTTOM_HEIGHT;

    gpu::setViewport(screen, 0, 0, width, height);
    gput::setOrtho(0, width, 0, height, -1, 1);

    gpu::clear();
    gput::drawString(str, (width - gput::getStringWidth(str, 8)) / 2, (height - gput::getStringHeight(str, 8)) / 2, 8, 8);
    gpu::flushCommands();
    gpu::flushBuffer();
    gpu::swapBuffers(!quickSwap);

    gpu::setViewport(gpu::SCREEN_TOP, 0, 0, gpu::TOP_WIDTH, gpu::TOP_HEIGHT);
    gput::setOrtho(0, gpu::TOP_WIDTH, 0, gpu::TOP_HEIGHT, -1, 1);
}

u64 ntohll(u64 value) {
    static const int num = 42;
    if(*((char*) &num) == num) {
        return (((uint64_t) htonl((u32) value)) << 32) + htonl((u32) (value >> 32));
    } else {
        return value;
    }
}

int socketListen(u16 port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if(fd < 0) {
        return -1;
    }
    int rcvbuf = 32768;
    if(setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf)) < 0) {
        
    }
    
    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(port);

    if(bind(fd, (struct sockaddr*) &address, sizeof(address)) != 0) {
        closesocket(fd);
        return -1;
    }

    int flags = fcntl(fd, F_GETFL);
    if(flags == -1) {
        closesocket(fd);
        return -1;
    }

    if(fcntl(fd, F_SETFL, flags | O_NONBLOCK) != 0) {
        closesocket(fd);
        return -1;
    }

    if(listen(fd, 10) != 0) {
        closesocket(fd);
        return -1;
    }

    return fd;
}

FILE* socketAccept(int listeningSocket) {
    struct sockaddr_in addr;
    socklen_t addrSize = sizeof(addr);
    int afd = accept(listeningSocket, (struct sockaddr*) &addr, &addrSize);
    if(afd < 0) {
        return NULL;
    }

    int flags = fcntl(afd, F_GETFL);
    if(flags == -1) {
        closesocket(afd);
        return NULL;
    }

    if(fcntl(afd, F_SETFL, flags | O_NONBLOCK) != 0) {
        closesocket(afd);
        return NULL;
    }

    return fdopen(afd, "rw");
}

RemoteFile uiAcceptRemoteFile(gpu::Screen screen, std::function<void(std::stringstream& infoStream)> onWait) {
    // meh
    for(u32 i = 0; i < 2; i++) {
        uiDisplayMessage(screen == gpu::SCREEN_TOP ? gpu::SCREEN_BOTTOM : gpu::SCREEN_TOP, "");
    }

    uiDisplayMessage(screen, "Initializing socket...");

    int listen = socketListen(5000);
    if(listen < 0) {
        std::stringstream errStream;
        errStream << "Failed to initialize socket." << "\n" << strerror(errno) << "\n";
        uiPrompt(screen, errStream.str(), false);
        return {NULL, 0};
    }

    std::stringstream baseInfoStream;
    baseInfoStream << "Waiting for peer to connect..." << "\n";
    baseInfoStream << "IP: " << inet_ntoa({(u32) gethostid()}) << "\n";
    baseInfoStream << "Press B to cancel." << "\n";
    std::string baseInfo = baseInfoStream.str();

    FILE* socket;
    while((socket = socketAccept(listen)) == NULL) {
        if(!core::running()) {
            close(listen);
            return {NULL, 0};
        }

        if(errno != EAGAIN && errno != EWOULDBLOCK && errno != EINPROGRESS) {
            close(listen);

            std::stringstream errStream;
            errStream << "Failed to accept peer." << "\n" << strerror(errno) << "\n";
            uiPrompt(screen, errStream.str(), false);

            return {NULL, 0};
        }

        hid::poll();
        if(hid::pressed(hid::BUTTON_B)) {
            close(listen);
            return {NULL, 0};
        }

        std::stringstream infoStream;
        infoStream << baseInfo;
        onWait(infoStream);
        uiDisplayMessage(screen, infoStream.str());
    }

    close(listen);

    uiDisplayMessage(screen, "Reading info...\nPress B to cancel.");

    u64 fileSize = 0;
    u64 bytesRead = 0;
    while(bytesRead < sizeof(fileSize)) {
        if(!core::running()) {
            fclose(socket);
            return {NULL, 0};
        }

        size_t currBytesRead = fread(&fileSize + bytesRead, 1, (size_t) (sizeof(fileSize) - bytesRead), socket);
        if(currBytesRead > 0) {
            bytesRead += currBytesRead;
        }

        if(ferror(socket) && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINPROGRESS) {
            fclose(socket);

            std::stringstream errStream;
            errStream << "Failed to read info." << "\n" << strerror(errno) << "\n";
            uiPrompt(screen, errStream.str(), false);

            return {NULL, 0};
        }

        hid::poll();
        if(hid::pressed(hid::BUTTON_B)) {
            fclose(socket);
            return {NULL, 0};
        }
    }

    fileSize = ntohll(fileSize);
    return {socket, fileSize};
}
