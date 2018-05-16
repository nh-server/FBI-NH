#pragma once

typedef struct SMDH_s SMDH;

Result cia_get_title_id(u64* titleId, u8* cia, size_t size);
Result cia_file_get_smdh(SMDH* smdh, Handle handle);