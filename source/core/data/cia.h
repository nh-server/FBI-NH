#pragma once

typedef struct SMDH_s SMDH;

u64 cia_get_title_id(u8* cia);
Result cia_file_get_smdh(SMDH* smdh, Handle handle);