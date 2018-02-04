#pragma once

u64 tmd_get_title_id(u8* tmd);
u16 tmd_get_content_count(u8* tmd);
u8* tmd_get_content_chunk(u8* tmd, u32 index);