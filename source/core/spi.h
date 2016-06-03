#pragma once

Result spi_init_card();
Result spi_deinit_card();
Result spi_get_save_size(u32* size);
Result spi_read_save(u32* bytesRead, void* data, u32 offset, u32 size);
Result spi_write_save(u32* bytesWritten, void* data, u32 offset, u32 size);