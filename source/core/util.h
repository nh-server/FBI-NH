#pragma once

typedef struct {
    u16 shortDescription[0x40];
    u16 longDescription[0x80];
    u16 publisher[0x40];
} SMDH_title;

typedef struct {
    char magic[0x04];
    u16 version;
    u16 reserved1;
    SMDH_title titles[0x10];
    u8 ratings[0x10];
    u32 region;
    u32 matchMakerId;
    u64 matchMakerBitId;
    u32 flags;
    u16 eulaVersion;
    u16 reserved;
    u32 optimalBannerFrame;
    u32 streetpassId;
    u64 reserved2;
    u8 smallIcon[0x480];
    u8 largeIcon[0x1200];
} SMDH;

typedef struct {
    u8 version;
    bool animated;
    u16 crc16[4];
    u8 reserved[0x16];
    u8 mainIconBitmap[0x200];
    u16 mainIconPalette[0x10];
    u16 titles[16][0x80];
    u8 animatedFrameBitmaps[8][0x200];
    u16 animatedFramePalettes[8][0x10];
    u16 animationSequence[0x40];
} BNR;

void util_panic(const char* s, ...);

FS_Path* util_make_path_utf8(const char* path);
void util_free_path_utf8(FS_Path* path);

FS_Path util_make_binary_path(const void* data, u32 size);

bool util_is_dir(FS_Archive archive, const char* path);
Result util_ensure_dir(FS_Archive archive, const char* path);

void util_get_path_file(char* out, const char* path, u32 size);
void util_get_parent_path(char* out, const char* path, u32 size);

bool util_is_string_empty(const char* str);

Result util_import_seed(u32* responseCode, u64 titleId);

FS_MediaType util_get_title_destination(u64 titleId);

u64 util_get_cia_title_id(u8* cia);
Result util_get_cia_file_smdh(SMDH* smdh, Handle handle);
u64 util_get_ticket_title_id(u8* ticket);
u64 util_get_tmd_title_id(u8* tmd);
u16 util_get_tmd_content_count(u8* tmd);
u8* util_get_tmd_content_chunk(u8* tmd, u32 index);

bool util_filter_cias(void* data, const char* name, u32 attributes);
bool util_filter_tickets(void* data, const char* name, u32 attributes);

int util_compare_file_infos(void* userData, const void* p1, const void* p2);

const char* util_get_3dsx_path();
void util_set_3dsx_path(const char* path);

Result util_open_archive(FS_Archive* archive, FS_ArchiveID id, FS_Path path);
Result util_ref_archive(FS_Archive archive);
Result util_close_archive(FS_Archive archive);

const char* util_get_display_eta(u32 seconds);
double util_get_display_size(u64 size);
const char* util_get_display_size_units(u64 size);

void util_escape_file_name(char* out, const char* in, size_t size);

void util_smdh_region_to_string(char* out, u32 region, size_t size);

SMDH_title* util_select_smdh_title(SMDH* smdh);
u16* util_select_bnr_title(BNR* bnr);

Result util_http_open(httpcContext* context, u32* responseCode, const char* url, bool userAgent);
Result util_http_open_ranged(httpcContext* context, u32* responseCode, const char* url, bool userAgent, u32 rangeStart, u32 rangeEnd);
Result util_http_get_size(httpcContext* context, u32* size);
Result util_http_read(httpcContext* context, u32* bytesRead, void* buffer, u32 size);
Result util_http_close(httpcContext* context);