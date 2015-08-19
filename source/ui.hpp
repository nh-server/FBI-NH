#pragma once

#include <citrus/app.hpp>
#include <citrus/fs.hpp>
#include <citrus/gpu.hpp>
#include <citrus/types.hpp>

#include <functional>
#include <string>
#include <vector>

typedef struct {
    std::string id;
    std::string name;
    std::vector<std::string> details;
} SelectableElement;

typedef struct {
    FILE* fd;
    u64 fileSize;
} RemoteFile;

void uiInit();
void uiCleanup();

bool uiSelect(SelectableElement* selected, std::vector<SelectableElement> elements, std::function<bool(std::vector<SelectableElement> &currElements, SelectableElement currElement, bool &elementsDirty, bool &resetCursorIfDirty)> onLoop, std::function<bool(SelectableElement select)> onSelect, bool useTopScreen = true, bool alphabetize = true, bool dpadPageScroll = true);
bool uiSelectFile(std::string* selectedFile, const std::string rootDirectory, std::vector<std::string> extensions, std::function<bool(const std::string currDirectory, bool inRoot, bool &updateList)> onLoop, std::function<bool(const std::string path, bool &updateList)> onSelect, bool useTopScreen = true, bool dpadPageScroll = true);
bool uiSelectApp(ctr::app::App* selectedApp, ctr::fs::MediaType mediaType, std::function<bool(bool &updateList)> onLoop, std::function<bool(ctr::app::App app, bool &updateList)> onSelect, bool useTopScreen = true, bool dpadPageScroll = true);
void uiDisplayMessage(ctr::gpu::Screen screen, const std::string message);
bool uiPrompt(ctr::gpu::Screen screen, const std::string message, bool question);
void uiDisplayProgress(ctr::gpu::Screen screen, const std::string operation, const std::string details, bool quickSwap, u32 progress);
RemoteFile uiAcceptRemoteFile(ctr::gpu::Screen screen, std::function<void(std::stringstream& infoStream)> onWait = [&](std::stringstream& infoStream){});
