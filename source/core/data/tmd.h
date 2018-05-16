#pragma once

Result tmd_get_title_id(u64* titleId, u8* tmd, size_t size);
Result tmd_get_content_count(u16* contentCount, u8* tmd, size_t size);
Result tmd_get_content_id(u32* id, u8* tmd, size_t size, u32 num);
Result tmd_get_content_index(u16* index, u8* tmd, size_t size, u32 num);