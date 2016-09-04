#include <arpa/inet.h>
#include <sys/unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <malloc.h>
#include <stdio.h>
#include <string.h>

#include <3ds.h>

#include "section.h"
#include "action/action.h"
#include "task/task.h"
#include "../error.h"
#include "../info.h"
#include "../prompt.h"
#include "../ui.h"
#include "../../core/screen.h"
#include "../../core/util.h"

typedef struct {
    int serverSocket;
    int clientSocket;

    bool cdn;
    bool cdnDecided;

    bool ticket;
    u64 currTitleId;
    volatile bool n3dsContinue;
    ticket_info ticketInfo;

    data_op_data installInfo;
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

static void networkinstall_cdn_check_onresponse(ui_view* view, void* data, bool response) {
    network_install_data* qrInstallData = (network_install_data*) data;

    qrInstallData->cdn = response;
    qrInstallData->cdnDecided = true;
}

static void networkinstall_n3ds_onresponse(ui_view* view, void* data, bool response) {
    ((network_install_data*) data)->n3dsContinue = response;
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
        return R_FBI_ERRNO;
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
        return R_FBI_ERRNO;
    }

    *size = __builtin_bswap64(netSize);
    return 0;
}

static Result networkinstall_read_src(void* data, u32 handle, u32* bytesRead, void* buffer, u64 offset, u32 size) {
    network_install_data* networkInstallData = (network_install_data*) data;

    int ret = 0;
    if((ret = recvwait(networkInstallData->clientSocket, buffer, size, 0)) < 0) {
        return R_FBI_ERRNO;
    }

    *bytesRead = (u32) ret;
    return 0;
}

static Result networkinstall_open_dst(void* data, u32 index, void* initialReadBlock, u64 size, u32* handle) {
    network_install_data* networkInstallData = (network_install_data*) data;

    Result res = 0;

    networkInstallData->ticket = false;
    networkInstallData->currTitleId = 0;
    networkInstallData->n3dsContinue = false;
    memset(&networkInstallData->ticketInfo, 0, sizeof(networkInstallData->ticketInfo));

    if(*(u16*) initialReadBlock == 0x0100) {
        if(!networkInstallData->cdnDecided) {
            ui_view* view = prompt_display("Optional", "Install ticket titles from CDN?", COLOR_TEXT, true, data, NULL, networkinstall_cdn_check_onresponse);
            if(view != NULL) {
                svcWaitSynchronization(view->active, U64_MAX);
            }
        }

        networkInstallData->ticket = true;
        networkInstallData->ticketInfo.titleId = util_get_ticket_title_id((u8*) initialReadBlock);

        AM_DeleteTicket(networkInstallData->ticketInfo.titleId);
        res = AM_InstallTicketBegin(handle);
    } else if(*(u16*) initialReadBlock == 0x2020) {
        u64 titleId = util_get_cia_title_id((u8*) initialReadBlock);

        FS_MediaType dest = util_get_title_destination(titleId);

        bool n3ds = false;
        if(R_SUCCEEDED(APT_CheckNew3DS(&n3ds)) && !n3ds && ((titleId >> 28) & 0xF) == 2) {
            ui_view* view = prompt_display("Confirmation", "Title is intended for New 3DS systems.\nContinue?", COLOR_TEXT, true, data, NULL, networkinstall_n3ds_onresponse);
            if(view != NULL) {
                svcWaitSynchronization(view->active, U64_MAX);
            }

            if(!networkInstallData->n3dsContinue) {
                return R_FBI_WRONG_SYSTEM;
            }
        }

        // Deleting FBI before it reinstalls itself causes issues.
        if(((titleId >> 8) & 0xFFFFF) != 0xF8001) {
            AM_DeleteTitle(dest, titleId);
            AM_DeleteTicket(titleId);

            if(dest == MEDIATYPE_SD) {
                AM_QueryAvailableExternalTitleDatabase(NULL);
            }
        }

        if(R_SUCCEEDED(res = AM_StartCiaInstall(dest, handle))) {
            networkInstallData->currTitleId = titleId;
        }
    } else {
        res = R_FBI_BAD_DATA;
    }

    return res;
}

static Result networkinstall_close_dst(void* data, u32 index, bool succeeded, u32 handle) {
    network_install_data* networkInstallData = (network_install_data*) data;

    if(succeeded) {
        Result res = 0;

        if(networkInstallData->ticket) {
            res = AM_InstallTicketFinish(handle);

            if(R_SUCCEEDED(res) && networkInstallData->cdn) {
                volatile bool done = false;
                action_install_cdn_noprompt(&done, &networkInstallData->ticketInfo, false);

                while(!done) {
                    svcSleepThread(100000000);
                }
            }
        } else {
            if(R_SUCCEEDED(res = AM_FinishCiaInstall(handle))) {
                util_import_seed(networkInstallData->currTitleId);

                if(networkInstallData->currTitleId == 0x0004013800000002 || networkInstallData->currTitleId == 0x0004013820000002) {
                    res = AM_InstallFirm(networkInstallData->currTitleId);
                }
            }
        }

        return res;
    } else {
        if(networkInstallData->ticket) {
            return AM_InstallTicketAbort(handle);
        } else {
            return AM_CancelCIAInstall(handle);
        }
    }
}

static Result networkinstall_write_dst(void* data, u32 handle, u32* bytesWritten, void* buffer, u64 offset, u32 size) {
    return FSFILE_Write(handle, bytesWritten, offset, buffer, size, 0);
}

static Result networkinstall_suspend_copy(void* data, u32 index, u32* srcHandle, u32* dstHandle) {
    return 0;
}

static Result networkinstall_restore_copy(void* data, u32 index, u32* srcHandle, u32* dstHandle) {
    return 0;
}

static Result networkinstall_suspend(void* data, u32 index) {
    return 0;
}

static Result networkinstall_restore(void* data, u32 index) {
    return 0;
}

static bool networkinstall_error(void* data, u32 index, Result res) {
    if(res == R_FBI_CANCELLED) {
        prompt_display("Failure", "Install cancelled.", COLOR_TEXT, false, NULL, NULL, NULL);
    } else if(res == R_FBI_ERRNO) {
        error_display_errno(NULL, NULL, errno, "Failed to install over the network.");
    } else if(res != R_FBI_WRONG_SYSTEM) {
        error_display_res(NULL, NULL, res, "Failed to install over the network.");
    }

    return false;
}

static void networkinstall_close_client(network_install_data* data) {
    if(data->clientSocket != 0) {
        u8 ack = 0;
        sendwait(data->clientSocket, &ack, sizeof(ack), 0);

        close(data->clientSocket);
        data->clientSocket = 0;
    }

    data->cdn = false;
    data->cdnDecided = false;

    data->ticket = false;
    data->currTitleId = 0;
    data->n3dsContinue = false;
    memset(&data->ticketInfo, 0, sizeof(data->ticketInfo));
}

static void networkinstall_free_data(network_install_data* data) {
    networkinstall_close_client(data);

    if(data->serverSocket != 0) {
        close(data->serverSocket);
        data->serverSocket = 0;
    }

    free(data);
}

static void networkinstall_install_update(ui_view* view, void* data, float* progress, char* text) {
    network_install_data* networkInstallData = (network_install_data*) data;

    if(networkInstallData->installInfo.finished) {
        networkinstall_close_client(networkInstallData);

        ui_pop();
        info_destroy(view);

        if(R_SUCCEEDED(networkInstallData->installInfo.result)) {
            prompt_display("Success", "Install finished.", COLOR_TEXT, false, NULL, NULL, NULL);
        }

        return;
    }

    if((hidKeysDown() & KEY_B) && !networkInstallData->installInfo.finished) {
        svcSignalEvent(networkInstallData->installInfo.cancelEvent);
    }

    *progress = networkInstallData->installInfo.currTotal != 0 ? (float) ((double) networkInstallData->installInfo.currProcessed / (double) networkInstallData->installInfo.currTotal) : 0;
    snprintf(text, PROGRESS_TEXT_MAX, "%lu / %lu\n%.2f %s / %.2f %s", networkInstallData->installInfo.processed, networkInstallData->installInfo.total, util_get_display_size(networkInstallData->installInfo.currProcessed), util_get_display_size_units(networkInstallData->installInfo.currProcessed), util_get_display_size(networkInstallData->installInfo.currTotal), util_get_display_size_units(networkInstallData->installInfo.currTotal));
}

static void networkinstall_confirm_onresponse(ui_view* view, void* data, bool response) {
    network_install_data* networkInstallData = (network_install_data*) data;

    if(response) {
        Result res = task_data_op(&networkInstallData->installInfo);
        if(R_SUCCEEDED(res)) {
            info_display("Installing Received File(s)", "Press B to cancel.", true, data, networkinstall_install_update, NULL);
        } else {
            error_display_res(NULL, NULL, res, "Failed to initiate installation.");

            networkinstall_close_client(networkInstallData);
        }
    } else {
        networkinstall_close_client(networkInstallData);
    }
}

static void networkinstall_wait_update(ui_view* view, void* data, float* progress, char* text) {
    network_install_data* networkInstallData = (network_install_data*) data;

    if(hidKeysDown() & KEY_B) {
        ui_pop();
        info_destroy(view);

        networkinstall_free_data(networkInstallData);

        return;
    }

    struct sockaddr_in client;
    socklen_t clientLen = sizeof(client);

    int sock = accept(networkInstallData->serverSocket, (struct sockaddr*) &client, &clientLen);
    if(sock >= 0) {
        if(recvwait(sock, &networkInstallData->installInfo.total, sizeof(networkInstallData->installInfo.total), 0) < 0) {
            close(sock);

            error_display_errno(NULL, NULL, errno, "Failed to read file count.");
            return;
        }

        networkInstallData->installInfo.total = ntohl(networkInstallData->installInfo.total);

        networkInstallData->clientSocket = sock;
        prompt_display("Confirmation", "Install the received file(s)?", COLOR_TEXT, true, data, NULL, networkinstall_confirm_onresponse);
    } else if(errno != EAGAIN) {
        if(errno == 22 || errno == 115) {
            ui_pop();
            info_destroy(view);
        }

        error_display_errno(NULL, NULL, errno, "Failed to open socket.");

        if(errno == 22 || errno == 115) {
            networkinstall_free_data(networkInstallData);

            return;
        }
    }

    struct in_addr addr = {(in_addr_t) gethostid()};
    snprintf(text, PROGRESS_TEXT_MAX, "Waiting for connection...\nIP: %s\nPort: 5000", inet_ntoa(addr));
}

void networkinstall_open() {
    network_install_data* data = (network_install_data*) calloc(1, sizeof(network_install_data));
    if(data == NULL) {
        error_display(NULL, NULL, "Failed to allocate network install data.");

        return;
    }

    data->clientSocket = 0;

    data->cdn = false;
    data->cdnDecided = false;

    data->ticket = false;
    data->currTitleId = 0;
    data->n3dsContinue = false;
    memset(&data->ticketInfo, 0, sizeof(data->ticketInfo));

    data->installInfo.data = data;

    data->installInfo.op = DATAOP_COPY;

    data->installInfo.copyBufferSize = 256 * 1024;
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

    data->installInfo.suspendCopy = networkinstall_suspend_copy;
    data->installInfo.restoreCopy = networkinstall_restore_copy;

    data->installInfo.suspend = networkinstall_suspend;
    data->installInfo.restore = networkinstall_restore;

    data->installInfo.error = networkinstall_error;

    data->installInfo.finished = true;

    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if(sock < 0) {
        error_display_errno(NULL, NULL, errno, "Failed to open server socket.");

        networkinstall_free_data(data);
        return;
    }

    data->serverSocket = sock;

    int bufSize = 1024 * 32;
    setsockopt(data->serverSocket, SOL_SOCKET, SO_RCVBUF, &bufSize, sizeof(bufSize));

    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(5000);
    server.sin_addr.s_addr = (in_addr_t) gethostid();

    if(bind(data->serverSocket, (struct sockaddr*) &server, sizeof(server)) < 0) {
        error_display_errno(NULL, NULL, errno, "Failed to bind server socket.");

        networkinstall_free_data(data);
        return;
    }

    fcntl(data->serverSocket, F_SETFL, fcntl(data->serverSocket, F_GETFL, 0) | O_NONBLOCK);

    if(listen(data->serverSocket, 5) < 0) {
        error_display_errno(NULL, NULL, errno, "Failed to listen on server socket.");

        networkinstall_free_data(data);
        return;
    }

    info_display("Network Install", "B: Return", false, data, networkinstall_wait_update, NULL);
}
