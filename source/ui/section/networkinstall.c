#include <arpa/inet.h>
#include <sys/unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <malloc.h>
#include <stdio.h>

#include <3ds.h>

#include "action/action.h"
#include "section.h"
#include "../error.h"
#include "../progressbar.h"
#include "../prompt.h"
#include "../../screen.h"
#include "../../util.h"

typedef struct {
    FS_MediaType dest;
    int serverSocket;
    int clientSocket;
    bool installStarted;
    u64 currProcessed;
    u64 currTotal;
    u32 processed;
    u32 total;
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

    return ret < 0 ? ret : (int) read;
}

static Result networkinstall_read(void* data, u32* bytesRead, void* buffer, u32 size) {
    network_install_data* networkInstallData = (network_install_data*) data;

    Result res = 0;

    int ret = 0;
    if((ret = recvwait(networkInstallData->clientSocket, buffer, size, 0)) >= 0) {
        networkInstallData->currProcessed += ret;
        if(bytesRead != NULL) {
            *bytesRead = (u32) ret;
        }
    } else {
        res = -2;
    }

    return res;
}

static void networkinstall_done_onresponse(ui_view* view, void* data, bool response) {
    task_refresh_titles();

    prompt_destroy(view);
}

static void networkinstall_close_client(network_install_data* data) {
    u8 ack = 0;
    sendwait(data->clientSocket, &ack, sizeof(ack), 0);
    close(data->clientSocket);

    data->installStarted = false;
    data->processed = 0;
    data->total = 0;
    data->currProcessed = 0;
    data->currTotal = 0;
}

static void networkinstall_install_update(ui_view* view, void* data, float* progress, char* progressText) {
    network_install_data* networkInstallData = (network_install_data*) data;

    bool cancelled = false;
    if(hidKeysDown() & KEY_B) {
        task_cancel_cia_install();

        while(task_is_cia_installing()) {
            svcSleepThread(1000000);
        }

        cancelled = true;
    }

    if(!task_is_cia_installing()) {
        if(networkInstallData->installStarted || cancelled) {
            Result res = task_get_cia_install_result();
            if(R_FAILED(res)) {
                networkinstall_close_client(networkInstallData);

                progressbar_destroy(view);
                ui_pop();

                if(res == CIA_INSTALL_RESULT_CANCELLED) {
                    ui_push(prompt_create("Failure", "Install cancelled.", 0xFF000000, false, data, NULL, NULL, networkinstall_done_onresponse));
                } else if(res == CIA_INSTALL_RESULT_ERRNO) {
                    error_display_errno(NULL, NULL, task_get_cia_install_errno(), "Failed to install CIA file.");
                } else if(res == CIA_INSTALL_RESULT_WRONG_SYSTEM) {
                    ui_push(prompt_create("Failure", "Attempted to install to wrong system.", 0xFF000000, false, data, NULL, NULL, networkinstall_done_onresponse));
                } else {
                    error_display_res(NULL, NULL, res, "Failed to install CIA file.");
                }

                return;
            }

            networkInstallData->processed++;
        }

        networkInstallData->installStarted = true;

        if(networkInstallData->processed >= networkInstallData->total) {
            networkinstall_close_client(networkInstallData);

            progressbar_destroy(view);
            ui_pop();

            ui_push(prompt_create("Success", "Install finished.", 0xFF000000, false, data, NULL, NULL, networkinstall_done_onresponse));
            return;
        } else {
            networkInstallData->currProcessed = 0;
            networkInstallData->currTotal = 0;

            u8 ack = 1;
            if(sendwait(networkInstallData->clientSocket, &ack, sizeof(ack), 0) < 0) {
                networkinstall_close_client(networkInstallData);

                progressbar_destroy(view);
                ui_pop();

                error_display_errno(NULL, NULL, errno, "Failed to write CIA accept notification.");
                return;
            }

            if(recvwait(networkInstallData->clientSocket, &networkInstallData->currTotal, sizeof(networkInstallData->currTotal), 0) < 0) {
                networkinstall_close_client(networkInstallData);

                progressbar_destroy(view);
                ui_pop();

                error_display_errno(NULL, NULL, errno, "Failed to read file size.");
                return;
            }

            networkInstallData->currTotal = __builtin_bswap64(networkInstallData->currTotal);
            task_request_cia_install(networkInstallData->dest, networkInstallData->currTotal, networkInstallData, networkinstall_read);
        }
    }

    *progress = (float) ((double) networkInstallData->currProcessed / (double) networkInstallData->currTotal);
    snprintf(progressText, PROGRESS_TEXT_MAX, "%lu / %lu\n%.2f MB / %.2f MB", networkInstallData->processed, networkInstallData->total, networkInstallData->currProcessed / 1024.0 / 1024.0, networkInstallData->currTotal / 1024.0 / 1024.0);
}

static void networkinstall_confirm_onresponse(ui_view* view, void* data, bool response) {
    network_install_data* networkInstallData = (network_install_data*) data;

    prompt_destroy(view);

    if(response) {
        ui_view* progressView = progressbar_create("Installing CIA(s)", "Press B to cancel.", data, networkinstall_install_update, NULL);
        snprintf(progressbar_get_progress_text(progressView), PROGRESS_TEXT_MAX, "0 / %lu", networkInstallData->total);
        ui_push(progressView);
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

        if(recvwait(sock, &networkInstallData->total, sizeof(networkInstallData->total), 0) < 0) {
            close(sock);

            error_display_errno(NULL, NULL, errno, "Failed to read file count.");
            return;
        }

        networkInstallData->total = ntohl(networkInstallData->total);

        networkInstallData->clientSocket = sock;
        ui_push(prompt_create("Confirmation", "Install received CIA(s)?", 0xFF000000, true, data, NULL, NULL, networkinstall_confirm_onresponse));
    } else if(errno != EAGAIN) {
        error_display_errno(NULL, NULL, errno, "Failed to open socket.");
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
    screen_draw_string(text, textX, textY, 0.5f, 0.5f, 0xFF000000, false);
}

void networkinstall_open(FS_MediaType dest) {
    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if(sock < 0) {
        error_display_errno(NULL, NULL, errno, "Failed to open server socket.");
        return;
    }

    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(5000);
    server.sin_addr.s_addr = (in_addr_t) gethostid();

    if(bind(sock, (struct sockaddr*) &server, sizeof(server)) < 0) {
        close(sock);

        error_display_errno(NULL, NULL, errno, "Failed to bind server socket.");
        return;
    }

    fcntl(sock, F_SETFL, fcntl(sock, F_GETFL, 0) | O_NONBLOCK);

    if(listen(sock, 5) < 0) {
        close(sock);

        error_display_errno(NULL, NULL, errno, "Failed to listen on server socket.");
        return;
    }

    network_install_data* data = (network_install_data*) calloc(1, sizeof(network_install_data));
    data->dest = dest;
    data->serverSocket = sock;
    data->clientSocket = 0;
    data->installStarted = false;
    data->currProcessed = 0;
    data->currTotal = 0;
    data->processed = 0;
    data->total = 0;

    ui_view* view = (ui_view*) calloc(1, sizeof(ui_view));
    view->name = "Network Install";
    view->info = "B: Return";
    view->data = data;
    view->update = networkinstall_wait_update;
    view->drawTop = NULL;
    view->drawBottom = networkinstall_wait_draw_bottom;
    ui_push(view);
}

void networkinstall_open_sd() {
    networkinstall_open(MEDIATYPE_SD);
}

void networkinstall_open_nand() {
    networkinstall_open(MEDIATYPE_NAND);
}