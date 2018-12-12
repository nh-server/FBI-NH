#include <stdio.h>
#include <string.h>

#include <3ds.h>

#include "uitask.h"
#include "../resources.h"
#include "../../core/core.h"

void task_draw_meta_info(ui_view* view, void* data, float x1, float y1, float x2, float y2) {
    meta_info* info = (meta_info*) data;

    u32 metaInfoBoxShadowWidth;
    u32 metaInfoBoxShadowHeight;
    screen_get_texture_size(&metaInfoBoxShadowWidth, &metaInfoBoxShadowHeight, TEXTURE_META_INFO_BOX_SHADOW);

    float metaInfoBoxShadowX = x1 + (x2 - x1 - metaInfoBoxShadowWidth) / 2;
    float metaInfoBoxShadowY = y1 + (y2 - y1) / 4 - metaInfoBoxShadowHeight / 2;
    screen_draw_texture(TEXTURE_META_INFO_BOX_SHADOW, metaInfoBoxShadowX, metaInfoBoxShadowY, metaInfoBoxShadowWidth, metaInfoBoxShadowHeight);

    u32 metaInfoBoxWidth;
    u32 metaInfoBoxHeight;
    screen_get_texture_size(&metaInfoBoxWidth, &metaInfoBoxHeight, TEXTURE_META_INFO_BOX);

    float metaInfoBoxX = x1 + (x2 - x1 - metaInfoBoxWidth) / 2;
    float metaInfoBoxY = y1 + (y2 - y1) / 4 - metaInfoBoxHeight / 2;
    screen_draw_texture(TEXTURE_META_INFO_BOX, metaInfoBoxX, metaInfoBoxY, metaInfoBoxWidth, metaInfoBoxHeight);

    if(info->texture != 0) {
        u32 iconWidth;
        u32 iconHeight;
        screen_get_texture_size(&iconWidth, &iconHeight, info->texture);

        float iconX = metaInfoBoxX + (64 - iconWidth) / 2;
        float iconY = metaInfoBoxY + (metaInfoBoxHeight - iconHeight) / 2;
        screen_draw_texture(info->texture, iconX, iconY, iconWidth, iconHeight);
    }

    float metaTextX = metaInfoBoxX + 64;

    float shortDescriptionHeight;
    screen_get_string_size_wrap(NULL, &shortDescriptionHeight, info->shortDescription, 0.5f, 0.5f, metaInfoBoxX + metaInfoBoxWidth - 8 - metaTextX);

    float longDescriptionHeight;
    screen_get_string_size_wrap(NULL, &longDescriptionHeight, info->longDescription, 0.5f, 0.5f, metaInfoBoxX + metaInfoBoxWidth - 8 - metaTextX);

    float publisherHeight;
    screen_get_string_size_wrap(NULL, &publisherHeight, info->publisher, 0.5f, 0.5f, metaInfoBoxX + metaInfoBoxWidth - 8 - metaTextX);

    float shortDescriptionY = metaInfoBoxY + (64 - shortDescriptionHeight - 2 - longDescriptionHeight - 2 - publisherHeight) / 2;
    screen_draw_string_wrap(info->shortDescription, metaTextX, shortDescriptionY, 0.5f, 0.5f, COLOR_TEXT, false, metaInfoBoxX + metaInfoBoxWidth - 8);

    float longDescriptionY = shortDescriptionY + shortDescriptionHeight + 2;
    screen_draw_string_wrap(info->longDescription, metaTextX, longDescriptionY, 0.5f, 0.5f, COLOR_TEXT, false, metaInfoBoxX + metaInfoBoxWidth - 8);

    float publisherY = longDescriptionY + longDescriptionHeight + 2;
    screen_draw_string_wrap(info->publisher, metaTextX, publisherY, 0.5f, 0.5f, COLOR_TEXT, false, metaInfoBoxX + metaInfoBoxWidth - 8);
}

void task_draw_ext_save_data_info(ui_view* view, void* data, float x1, float y1, float x2, float y2) {
    ext_save_data_info* info = (ext_save_data_info*) data;

    if(info->hasMeta) {
        task_draw_meta_info(view, &info->meta, x1, y1, x2, y2);
    }

    char infoText[512];

    snprintf(infoText, sizeof(infoText),
             "Ext Save Data ID: %016llX\n"
                     "Shared: %s",
             info->extSaveDataId,
             info->shared ? "Yes" : "No");

    float infoWidth;
    screen_get_string_size(&infoWidth, NULL, infoText, 0.5f, 0.5f);

    float infoX = x1 + (x2 - x1 - infoWidth) / 2;
    float infoY = y1 + (y2 - y1) / 2 - 8;
    screen_draw_string(infoText, infoX, infoY, 0.5f, 0.5f, COLOR_TEXT, true);
}

void task_draw_file_info(ui_view* view, void* data, float x1, float y1, float x2, float y2) {
    file_info* info = (file_info*) data;

    char infoText[512];
    size_t infoTextPos = 0;

    if(strlen(info->name) > 48) {
        infoTextPos += snprintf(infoText + infoTextPos, sizeof(infoText) - infoTextPos, "Name: %.45s...\n", info->name);
    } else {
        infoTextPos += snprintf(infoText + infoTextPos, sizeof(infoText) - infoTextPos, "Name: %.48s\n", info->name);
    }

    infoTextPos += snprintf(infoText + infoTextPos, sizeof(infoText) - infoTextPos, "Attributes: ");

    if(info->attributes & (FS_ATTRIBUTE_DIRECTORY | FS_ATTRIBUTE_HIDDEN | FS_ATTRIBUTE_ARCHIVE | FS_ATTRIBUTE_READ_ONLY)) {
        bool needsSeparator = false;

        if(info->attributes & FS_ATTRIBUTE_DIRECTORY) {
            infoTextPos += snprintf(infoText + infoTextPos, sizeof(infoText) - infoTextPos, "Directory");
            needsSeparator = true;
        }

        if(info->attributes & FS_ATTRIBUTE_HIDDEN) {
            if(needsSeparator) {
                infoTextPos += snprintf(infoText + infoTextPos, sizeof(infoText) - infoTextPos, ", ");
            }

            infoTextPos += snprintf(infoText + infoTextPos, sizeof(infoText) - infoTextPos, "Hidden");
            needsSeparator = true;
        }

        if(info->attributes & FS_ATTRIBUTE_ARCHIVE) {
            if(needsSeparator) {
                infoTextPos += snprintf(infoText + infoTextPos, sizeof(infoText) - infoTextPos, ", ");
            }

            infoTextPos += snprintf(infoText + infoTextPos, sizeof(infoText) - infoTextPos, "Archive");
            needsSeparator = true;
        }

        if(info->attributes & FS_ATTRIBUTE_READ_ONLY) {
            if(needsSeparator) {
                infoTextPos += snprintf(infoText + infoTextPos, sizeof(infoText) - infoTextPos, ", ");
            }

            infoTextPos += snprintf(infoText + infoTextPos, sizeof(infoText) - infoTextPos, "Read Only");
            needsSeparator = true;
        }
    } else {
        infoTextPos += snprintf(infoText + infoTextPos, sizeof(infoText) - infoTextPos, "None");
    }

    infoTextPos += snprintf(infoText + infoTextPos, sizeof(infoText) - infoTextPos, "\n");

    if(!(info->attributes & FS_ATTRIBUTE_DIRECTORY)) {
        infoTextPos += snprintf(infoText + infoTextPos, sizeof(infoText) - infoTextPos, "Size: %.2f %s\n",
                                ui_get_display_size(info->size), ui_get_display_size_units(info->size));

        if(info->isCia && info->ciaInfo.loaded) {
            char regionString[64];

            if(info->ciaInfo.hasMeta) {
                task_draw_meta_info(view, &info->ciaInfo.meta, x1, y1, x2, y2);

                smdh_region_to_string(regionString, info->ciaInfo.meta.region, sizeof(regionString));
            } else {
                snprintf(regionString, sizeof(regionString), "Unknown");
            }

            infoTextPos += snprintf(infoText + infoTextPos, sizeof(infoText) - infoTextPos,
                                    "Title ID: %016llX\n"
                                            "Version: %hu (%d.%d.%d)\n"
                                            "Region: %s\n"
                                            "Installed Size: %.2f %s",
                                    info->ciaInfo.titleId,
                                    info->ciaInfo.version, (info->ciaInfo.version >> 10) & 0x3F, (info->ciaInfo.version >> 4) & 0x3F, info->ciaInfo.version & 0xF,
                                    regionString,
                                    ui_get_display_size(info->ciaInfo.installedSize),
                                    ui_get_display_size_units(info->ciaInfo.installedSize));
        } else if(info->isTicket && info->ticketInfo.loaded) {
            infoTextPos += snprintf(infoText + infoTextPos, sizeof(infoText) - infoTextPos, "Ticket ID: %016llX", info->ticketInfo.titleId);
        }
    }

    float infoWidth;
    screen_get_string_size(&infoWidth, NULL, infoText, 0.5f, 0.5f);

    float infoX = x1 + (x2 - x1 - infoWidth) / 2;
    float infoY = y1 + (y2 - y1) / 2 - 8;
    screen_draw_string(infoText, infoX, infoY, 0.5f, 0.5f, COLOR_TEXT, true);
}

void task_draw_pending_title_info(ui_view* view, void* data, float x1, float y1, float x2, float y2) {
    pending_title_info* info = (pending_title_info*) data;

    char infoText[512];

    snprintf(infoText, sizeof(infoText),
             "Pending Title ID: %016llX\n"
                     "Media Type: %s\n"
                     "Version: %hu (%d.%d.%d)",
             info->titleId,
             info->mediaType == MEDIATYPE_NAND ? "NAND" : info->mediaType == MEDIATYPE_SD ? "SD" : "Game Card",
             info->version, (info->version >> 10) & 0x3F, (info->version >> 4) & 0x3F, info->version & 0xF);

    float infoWidth;
    screen_get_string_size(&infoWidth, NULL, infoText, 0.5f, 0.5f);

    float infoX = x1 + (x2 - x1 - infoWidth) / 2;
    float infoY = y1 + (y2 - y1) / 2 - 8;
    screen_draw_string(infoText, infoX, infoY, 0.5f, 0.5f, COLOR_TEXT, true);
}

void task_draw_system_save_data_info(ui_view* view, void* data, float x1, float y1, float x2, float y2) {
    system_save_data_info* info = (system_save_data_info*) data;

    char infoText[512];

    snprintf(infoText, sizeof(infoText), "System Save Data ID: %08lX", info->systemSaveDataId);

    float infoWidth;
    screen_get_string_size(&infoWidth, NULL, infoText, 0.5f, 0.5f);

    float infoX = x1 + (x2 - x1 - infoWidth) / 2;
    float infoY = y1 + (y2 - y1) / 2 - 8;
    screen_draw_string(infoText, infoX, infoY, 0.5f, 0.5f, COLOR_TEXT, true);
}

void task_draw_ticket_info(ui_view* view, void* data, float x1, float y1, float x2, float y2) {
    ticket_info* info = (ticket_info*) data;

    if(info->loaded) {
        char infoText[512];

        snprintf(infoText, sizeof(infoText), "Title ID: %016llX", info->titleId);

        float infoWidth;
        screen_get_string_size(&infoWidth, NULL, infoText, 0.5f, 0.5f);

        float infoX = x1 + (x2 - x1 - infoWidth) / 2;
        float infoY = y1 + (y2 - y1) / 2 - 8;
        screen_draw_string(infoText, infoX, infoY, 0.5f, 0.5f, COLOR_TEXT, true);
    }
}

void task_draw_title_info(ui_view* view, void* data, float x1, float y1, float x2, float y2) {
    title_info* info = (title_info*) data;

    char regionString[64];

    if(info->hasMeta) {
        task_draw_meta_info(view, &info->meta, x1, y1, x2, y2);

        smdh_region_to_string(regionString, info->meta.region, sizeof(regionString));
    } else {
        snprintf(regionString, sizeof(regionString), "Unknown");
    }

    char infoText[512];

    snprintf(infoText, sizeof(infoText),
             "Title ID: %016llX\n"
                     "Media Type: %s\n"
                     "Version: %hu (%d.%d.%d)\n"
                     "Product Code: %s\n"
                     "Region: %s\n"
                     "Size: %.2f %s",
             info->titleId,
             info->mediaType == MEDIATYPE_NAND ? "NAND" : info->mediaType == MEDIATYPE_SD ? "SD" : "Game Card",
             info->version, (info->version >> 10) & 0x3F, (info->version >> 4) & 0x3F, info->version & 0xF,
             info->productCode,
             regionString,
             ui_get_display_size(info->installedSize), ui_get_display_size_units(info->installedSize));

    float infoWidth;
    screen_get_string_size(&infoWidth, NULL, infoText, 0.5f, 0.5f);

    float infoX = x1 + (x2 - x1 - infoWidth) / 2;
    float infoY = y1 + (y2 - y1) / 2 - 8;
    screen_draw_string(infoText, infoX, infoY, 0.5f, 0.5f, COLOR_TEXT, true);
}