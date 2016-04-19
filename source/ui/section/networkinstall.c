#include <arpa/inet.h>
#include <sys/unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <malloc.h>
#include <stdio.h>
#include <string.h>

#include <3ds.h>

#include "action/action.h"
#include "task/task.h"
#include "../error.h"
#include "../progressbar.h"
#include "../prompt.h"
#include "../../screen.h"
#include "../../util.h"
#include "section.h"

typedef struct {
    int serverSocket;
    int clientSocket;

    u64 currTitleId;

    data_op_info installInfo;
    Handle cancelEvent;
} network_install_data;

static int recvwait(int sockfd, void* buf, size_t len, int flags) {
    errno = 0;

    int ret = 0;
    size_t read = 0;
    while(((ret = recv(sockfd, buf + read, len - read, flags)) >= 0 && (read += ret) < len) || errno == EAGAIN) {
        errno = 0;
    }

    return ret < 0 ? ret : (int) read;
}

static int sendwait(int sockfd, void* buf, size_t len, int flags) {
    errno = 0;

    int ret = 0;
    size_t written = 0;
    while(((ret = send(sockfd, buf + written, len - written, flags)) >= 0 && (written += ret) < len) || errno == EAGAIN) {
        errno = 0;
    }

    return ret < 0 ? ret : (int) written;
}

static Result networkinstall_is_src_directory(void* data, u32 index, bool* isDirectory) {
    *isDirectory = false;
    return 0;
}

static Result networkinstall_make_dst_directory(void* data, u32 index) {
    return 0;
}

static Result networkinstall_open_src(void* data, u32 index, u32* handle) {
    network_install_data* networkInstallData = (network_install_data*) data;

    u8 ack = 1;
    if(sendwait(networkInstallData->clientSocket, &ack, sizeof(ack), 0) < 0) {
        return -1;
    }

    return 0;
}

static Result networkinstall_close_src(void* data, u32 index, bool succeeded, u32 handle) {
    return 0;
}

static Result networkinstall_get_src_size(void* data, u32 handle, u64* size) {
    network_install_data* networkInstallData = (network_install_data*) data;

    u64 netSize = 0;
    if(recvwait(networkInstallData->clientSocket, &netSize, sizeof(netSize), 0) < 0) {
        return -1;
    }

    *size = __builtin_bswap64(netSize);
    return 0;
}

static Result networkinstall_read_src(void* data, u32 handle, u32* bytesRead, void* buffer, u64 offset, u32 size) {
    network_install_data* networkInstallData = (network_install_data*) data;

    int ret = 0;
    if((ret = recvwait(networkInstallData->clientSocket, buffer, size, 0)) < 0) {
        return -1;
    }

    *bytesRead = (u32) ret;
    return 0;
}

static Result networkinstall_open_dst(void* data, u32 index, void* initialReadBlock, u32* handle) {
    network_install_data* networkInstallData = (network_install_data*) data;

    u8* buffer = (u8*) initialReadBlock;

    u32 headerSize = *(u32*) &buffer[0x00];
    u32 certSize = *(u32*) &buffer[0x08];
    u64 titleId = __builtin_bswap64(*(u64*) &buffer[((headerSize + 0x3F) & ~0x3F) + ((certSize + 0x3F) & ~0x3F) + 0x1DC]);

    FS_MediaType dest = ((titleId >> 32) & 0x8010) != 0 ? MEDIATYPE_NAND : MEDIATYPE_SD;

    u8 n3ds = false;
    if(R_SUCCEEDED(APT_CheckNew3DS(&n3ds)) && !n3ds && ((titleId >> 28) & 0xF) == 2) {
        return MAKERESULT(RL_PERMANENT, RS_NOTSUPPORTED, RM_APPLICATION, RD_INVALID_COMBINATION);
    }

    // Deleting FBI before it reinstalls itself causes issues.
    if(((titleId >> 8) & 0xFFFFF) != 0xF8001) {
        AM_DeleteTitle(dest, titleId);
        AM_DeleteTicket(titleId);

        if(dest == 1) {
            AM_QueryAvailableExternalTitleDatabase(NULL);
        }
    }

    Result res = AM_StartCiaInstall(dest, handle);
    if(R_SUCCEEDED(res)) {
        networkInstallData->currTitleId = titleId;
    }

    return res;
}

static Result networkinstall_close_dst(void* data, u32 index, bool succeeded, u32 handle) {
    if(succeeded) {
        network_install_data* networkInstallData = (network_install_data*) data;

        Result res = 0;
        if(R_SUCCEEDED(res = AM_FinishCiaInstall(handle))) {
            if(networkInstallData->currTitleId == 0x0004013800000002 || networkInstallData->currTitleId == 0x0004013820000002) {
                res = AM_InstallFirm(networkInstallData->currTitleId);
            }
        }

        return res;
    } else {
        return AM_CancelCIAInstall(handle);
    }
}

static Result networkinstall_write_dst(void* data, u32 handle, u32* bytesWritten, void* buffer, u64 offset, u32 size) {
    return FSFILE_Write(handle, bytesWritten, offset, buffer, size, 0);
}

static bool networkinstall_result_error(void* data, u32 index, Result res) {
    network_install_data* networkInstallData = (network_install_data*) data;

    if(res == MAKERESULT(RL_PERMANENT, RS_CANCELED, RM_APPLICATION, RD_CANCEL_REQUESTED)) {
        ui_push(prompt_create("Failure", "Install cancelled.", COLOR_TEXT, false, NULL, NULL, NULL, NULL));
        return false;
    } else {
        volatile bool dismissed = false;
        if(res == MAKERESULT(RL_PERMANENT, RS_NOTSUPPORTED, RM_APPLICATION, RD_INVALID_COMBINATION)) {
            error_display(&dismissed, NULL, NULL, "Failed to install CIA file.\nAttempted to install N3DS title to O3DS.");
        } else {
            error_display_res(&dismissed, NULL, NULL, res, "Failed to install CIA file.");
        }

        while(!dismissed) {
            svcSleepThread(1000000);
        }
    }

    return index < networkInstallData->installInfo.total - 1;
}

static bool networkinstall_io_error(void* data, u32 index, int err) {
    network_install_data* networkInstallData = (network_install_data*) data;

    volatile bool dismissed = false;
    error_display_errno(&dismissed, NULL, NULL, err, "Failed to install CIA file.");

    while(!dismissed) {
        svcSleepThread(1000000);
    }

    return index < networkInstallData->installInfo.total - 1;
}

static void networkinstall_done_onresponse(ui_view* view, void* data, bool response) {
    prompt_destroy(view);
}

static void networkinstall_close_client(network_install_data* data) {
    u8 ack = 0;
    sendwait(data->clientSocket, &ack, sizeof(ack), 0);
    close(data->clientSocket);

    data->currTitleId = 0;
    data->cancelEvent = 0;
}

static void networkinstall_install_update(ui_view* view, void* data, float* progress, char* progressText) {
    network_install_data* networkInstallData = (network_install_data*) data;

    if(networkInstallData->installInfo.finished) {
        networkinstall_close_client(networkInstallData);

        ui_pop();
        progressbar_destroy(view);

        if(!networkInstallData->installInfo.premature) {
            ui_push(prompt_create("Success", "Install finished.", COLOR_TEXT, false, data, NULL, NULL, networkinstall_done_onresponse));
        }

        return;
    }

    if(hidKeysDown() & KEY_B) {
        svcSignalEvent(networkInstallData->cancelEvent);
    }

    *progress = networkInstallData->installInfo.currTotal != 0 ? (float) ((double) networkInstallData->installInfo.currProcessed / (double) networkInstallData->installInfo.currTotal) : 0;
    snprintf(progressText, PROGRESS_TEXT_MAX, "%lu / %lu\n%.2f MB / %.2f MB", networkInstallData->installInfo.processed, networkInstallData->installInfo.total, networkInstallData->installInfo.currProcessed / 1024.0 / 1024.0, networkInstallData->installInfo.currTotal / 1024.0 / 1024.0);
}

static void networkinstall_confirm_onresponse(ui_view* view, void* data, bool response) {
    prompt_destroy(view);

    network_install_data* networkInstallData = (network_install_data*) data;

    if(response) {
        networkInstallData->cancelEvent = task_data_op(&networkInstallData->installInfo);
        if(networkInstallData->cancelEvent != 0) {
            ui_view* progressView = progressbar_create("Installing CIA(s)", "Press B to cancel.", data, networkinstall_install_update, NULL);
            snprintf(progressbar_get_progress_text(progressView), PROGRESS_TEXT_MAX, "0 / %lu", networkInstallData->installInfo.total);
            ui_push(progressView);
        } else {
            error_display(NULL, NULL, NULL, "Failed to initiate CIA installation.");

            networkinstall_close_client(networkInstallData);
        }
    } else {
        networkinstall_close_client(networkInstallData);
    }
}

static void networkinstall_wait_update(ui_view* view, void* data, float bx1, float by1, float bx2, float by2) {
    network_install_data* networkInstallData = (network_install_data*) data;

    if(hidKeysDown() & KEY_B) {
        close(networkInstallData->serverSocket);

        free(networkInstallData);
        free(view);
        ui_pop();

        return;
    }

    struct sockaddr_in client;
    socklen_t clientLen = sizeof(client);

    int sock = accept(networkInstallData->serverSocket, (struct sockaddr*) &client, &clientLen);
    if(sock >= 0) {
        int bufSize = 1024 * 32;
        setsockopt(sock, SOL_SOCKET, SO_RCVBUF, &bufSize, sizeof(bufSize));

        if(recvwait(sock, &networkInstallData->installInfo.total, sizeof(networkInstallData->installInfo.total), 0) < 0) {
            close(sock);

            error_display_errno(NULL, NULL, NULL, errno, "Failed to read file count.");
            return;
        }

        networkInstallData->installInfo.total = ntohl(networkInstallData->installInfo.total);

        networkInstallData->clientSocket = sock;
        ui_push(prompt_create("Confirmation", "Install received CIA(s)?", COLOR_TEXT, true, data, NULL, NULL, networkinstall_confirm_onresponse));
    } else if(errno != EAGAIN) {
        error_display_errno(NULL, NULL, NULL, errno, "Failed to open socket.");
    }
}

static void networkinstall_wait_draw_bottom(ui_view* view, void* data, float x1, float y1, float x2, float y2) {
    struct in_addr addr = {(in_addr_t) gethostid()};

    char text[128];
    snprintf(text, 128, "Waiting for connection...\nIP: %s\nPort: 5000", inet_ntoa(addr));

    float textWidth;
    float textHeight;
    screen_get_string_size(&textWidth, &textHeight, text, 0.5f, 0.5f);

    float textX = x1 + (x2 - x1 - textWidth) / 2;
    float textY = y1 + (y2 - y1 - textHeight) / 2;
    screen_draw_string(text, textX, textY, 0.5f, 0.5f, COLOR_TEXT, false);
}

void networkinstall_open() {
    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if(sock < 0) {
        error_display_errno(NULL, NULL, NULL, errno, "Failed to open server socket.");
        return;
    }

    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(5000);
    server.sin_addr.s_addr = (in_addr_t) gethostid();

    if(bind(sock, (struct sockaddr*) &server, sizeof(server)) < 0) {
        close(sock);

        error_display_errno(NULL, NULL, NULL, errno, "Failed to bind server socket.");
        return;
    }

    fcntl(sock, F_SETFL, fcntl(sock, F_GETFL, 0) | O_NONBLOCK);

    if(listen(sock, 5) < 0) {
        close(sock);

        error_display_errno(NULL, NULL, NULL, errno, "Failed to listen on server socket.");
        return;
    }

    network_install_data* data = (network_install_data*) calloc(1, sizeof(network_install_data));
    data->serverSocket = sock;
    data->clientSocket = 0;

    data->currTitleId = 0;

    data->installInfo.data = data;

    data->installInfo.op = DATAOP_COPY;

    data->installInfo.copyEmpty = false;

    data->installInfo.isSrcDirectory = networkinstall_is_src_directory;
    data->installInfo.makeDstDirectory = networkinstall_make_dst_directory;

    data->installInfo.openSrc = networkinstall_open_src;
    data->installInfo.closeSrc = networkinstall_close_src;
    data->installInfo.getSrcSize = networkinstall_get_src_size;
    data->installInfo.readSrc = networkinstall_read_src;

    data->installInfo.openDst = networkinstall_open_dst;
    data->installInfo.closeDst = networkinstall_close_dst;
    data->installInfo.writeDst = networkinstall_write_dst;

    data->installInfo.resultError = networkinstall_result_error;
    data->installInfo.ioError = networkinstall_io_error;

    data->cancelEvent = 0;

    ui_view* view = (ui_view*) calloc(1, sizeof(ui_view));
    view->name = "Network Install";
    view->info = "B: Return";
    view->data = data;
    view->update = networkinstall_wait_update;
    view->drawTop = NULL;
    view->drawBottom = networkinstall_wait_draw_bottom;
    ui_push(view);
}
