#include <malloc.h>
#include <string.h>

#include <3ds.h>

#include "error.h"
#include "spi.h"

/*
 * Based on information from TWLSaveTool, by TuxSH.
 *  - https://github.com/TuxSH/TWLSaveTool/blob/master/source/SPI.cpp
 *
 * The original license is as follows:
 *
 *  Copyright (C) 2015-2016 TuxSH
 *
 *  TWLSaveTool is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>
 */

#define SPI_CMD_RDSR 5
#define SPI_CMD_WREN 6
#define SPI_CMD_RDID 0x9F

#define SPI_EEPROM_512B_CMD_WRLO 2
#define SPI_EEPROM_512B_CMD_RDLO 3
#define SPI_EEPROM_512B_CMD_WRHI 10
#define SPI_EEPROM_512B_CMD_RDHI 11

#define SPI_EEPROM_CMD_WRITE 2
#define SPI_EEPROM_CMD_READ 3

#define SPI_FLASH_CMD_READ 3
#define SPI_FLASH_CMD_PW 10

#define SPI_STAT_WIP 1
#define SPI_STAT_WEL 2

typedef enum {
    CHIP_NONE = 0,
    CHIP_EEPROM_512B = 1,
    CHIP_EEPROM_8KB = 2,
    CHIP_EEPROM_64KB = 3,
    CHIP_EEPROM_128KB = 4,
    CHIP_FLASH_256KB = 5,
    CHIP_FLASH_512KB = 6,
    CHIP_FLASH_1MB = 7,
    CHIP_FLASH_8MB = 8,
    CHIP_FLASH_256KB_INFRARED = 9,
    CHIP_FLASH_512KB_INFRARED = 10,
    CHIP_FLASH_1MB_INFRARED = 11,
    CHIP_FLASH_8MB_INFRARED = 12
} SaveChip;

static Result spi_get_page_size(SaveChip chip, u32* pageSize) {
    Result res = 0;

    u32 size = 0;
    switch(chip) {
        case CHIP_EEPROM_512B:
            size = 16;
            break;
        case CHIP_EEPROM_8KB:
            size = 32;
            break;
        case CHIP_EEPROM_64KB:
            size = 128;
            break;
        case CHIP_EEPROM_128KB:
        case CHIP_FLASH_256KB:
        case CHIP_FLASH_512KB:
        case CHIP_FLASH_1MB:
        case CHIP_FLASH_8MB:
        case CHIP_FLASH_256KB_INFRARED:
        case CHIP_FLASH_512KB_INFRARED:
        case CHIP_FLASH_1MB_INFRARED:
        case CHIP_FLASH_8MB_INFRARED:
            size = 256;
            break;
        default:
            res = R_APP_NOT_IMPLEMENTED;
            break;
    }

    if(R_SUCCEEDED(res) && pageSize != NULL) {
        *pageSize = size;
    }

    return res;
}

static Result spi_get_capacity(SaveChip chip, u32* capacity) {
    Result res = 0;

    u32 cap = 0;
    switch(chip) {
        case CHIP_EEPROM_512B:
            cap = 512;
            break;
        case CHIP_EEPROM_8KB:
            cap = 8 * 1024;
            break;
        case CHIP_EEPROM_64KB:
            cap = 64 * 1024;
            break;
        case CHIP_EEPROM_128KB:
            cap = 128 * 1024;
            break;
        case CHIP_FLASH_256KB:
        case CHIP_FLASH_256KB_INFRARED:
            cap = 256 * 1024;
            break;
        case CHIP_FLASH_512KB:
        case CHIP_FLASH_512KB_INFRARED:
            cap = 512 * 1024;
            break;
        case CHIP_FLASH_1MB:
        case CHIP_FLASH_1MB_INFRARED:
            cap = 1024 * 1024;
            break;
        case CHIP_FLASH_8MB:
        case CHIP_FLASH_8MB_INFRARED:
            cap = 8 * 1024 * 1024;
            break;
        default:
            res = R_APP_NOT_IMPLEMENTED;
            break;
    }

    if(R_SUCCEEDED(res) && capacity != NULL) {
        *capacity = cap;
    }

    return res;
}

static Result spi_execute_command(SaveChip chip, void* cmd, u32 cmdSize, void* answer, u32 answerSize, void* data, u32 dataSize) {
    if(chip == CHIP_NONE) {
        return R_APP_NOT_IMPLEMENTED;
    }

    bool infrared = chip == CHIP_FLASH_256KB_INFRARED || chip == CHIP_FLASH_512KB_INFRARED || chip == CHIP_FLASH_1MB_INFRARED || chip == CHIP_FLASH_8MB_INFRARED;

    u8 transferOp = pxiDevMakeTransferOption(BAUDRATE_4MHZ, BUSMODE_1BIT);
    u64 waitOp = pxiDevMakeWaitOperation(WAIT_NONE, DEASSERT_NONE, 0);

    u8 dummy = 0;
    PXIDEV_SPIBuffer header = {infrared ? &dummy : NULL, infrared ? 1 : 0, infrared ? pxiDevMakeTransferOption(BAUDRATE_1MHZ, BUSMODE_1BIT) : transferOp, waitOp};
    PXIDEV_SPIBuffer writeBuffer1 = {cmd, cmdSize, transferOp, waitOp};
    PXIDEV_SPIBuffer readBuffer1 = {answer, answerSize, transferOp, waitOp};
    PXIDEV_SPIBuffer writeBuffer2 = {data, dataSize, transferOp, waitOp};
    PXIDEV_SPIBuffer readBuffer2 = {NULL, 0, transferOp, waitOp};
    PXIDEV_SPIBuffer footer = {NULL, 0, transferOp, waitOp};

    return PXIDEV_SPIMultiWriteRead(&header, &writeBuffer1, &readBuffer1, &writeBuffer2, &readBuffer2, &footer);
}

static Result spi_wait_write_finish(SaveChip chip) {
    Result res = 0;

    u8 cmd = SPI_CMD_RDSR;
    u8 status = 0;
    while(R_SUCCEEDED(res = spi_execute_command(chip, &cmd, sizeof(cmd), &status, sizeof(status), NULL, 0)) && (status & SPI_STAT_WIP));

    return res;
}

static Result spi_read_jedec_id_status(SaveChip chip, u32* jecedId, u8* status) {
    Result res = 0;

    u8 cmd = SPI_CMD_RDID;
    u8 idData[3] = {0};
    if(R_SUCCEEDED(res = spi_wait_write_finish(chip)) && R_SUCCEEDED(res = spi_execute_command(chip, &cmd, sizeof(cmd), idData, sizeof(idData), NULL, 0))) {
        cmd = SPI_CMD_RDSR;
        u8 stat = 0;
        if(R_SUCCEEDED(res = spi_execute_command(chip, &cmd, 1, &stat, 1, 0, 0))) {
            if(jecedId != NULL) {
                *jecedId = (idData[0] << 16) | (idData[1] << 8) | idData[2];
            }

            if(status != NULL) {
                *status = stat;
            }
        }
    }

    return res;
}

static Result spi_read_data(SaveChip chip, u32* bytesRead, void* data, u32 offset, u32 size) {
    Result res = 0;

    u32 capacity = 0;
    if(R_SUCCEEDED(res = spi_get_capacity(chip, &capacity))) {
        if(size > capacity - offset) {
            size = capacity - offset;
        }

        u32 pos = offset;
        if(size > 0 && R_SUCCEEDED(res = spi_wait_write_finish(chip))) {
            u8 cmd[4] = {0};
            u32 cmdSize = 0;

            switch(chip) {
                case CHIP_EEPROM_512B:
                    if(pos < 0x100) {
                        u32 len = size > 0x100 - pos ? 0x100 - pos : size;

                        cmdSize = 2;
                        cmd[0] = SPI_EEPROM_512B_CMD_RDLO;
                        cmd[1] = (u8) pos;
                        res = spi_execute_command(chip, cmd, cmdSize, data, len, NULL, 0);

                        pos += len;

                        data = (u8*) data + len;
                        size = size - len;
                    }

                    if(R_SUCCEEDED(res) && pos >= 0x100 && size > 0) {
                        u32 len = size > 0x200 - pos ? 0x200 - pos : size;

                        cmdSize = 2;
                        cmd[0] = SPI_EEPROM_512B_CMD_RDHI;
                        cmd[1] = (u8) pos;
                        res = spi_execute_command(chip, cmd, cmdSize, data, len, NULL, 0);

                        pos += len;
                    }

                    break;
                case CHIP_EEPROM_8KB:
                case CHIP_EEPROM_64KB:
                    cmdSize = 3;
                    cmd[0] = SPI_EEPROM_CMD_READ;
                    cmd[1] = (u8) (pos >> 8);
                    cmd[2] = (u8) pos;
                    res = spi_execute_command(chip, cmd, cmdSize, data, size, NULL, 0);

                    pos += size;
                    break;
                case CHIP_EEPROM_128KB:
                    cmdSize = 4;
                    cmd[0] = SPI_EEPROM_CMD_READ;
                    cmd[1] = (u8) (pos >> 16);
                    cmd[2] = (u8) (pos >> 8);
                    cmd[3] = (u8) pos;
                    res = spi_execute_command(chip, cmd, cmdSize, data, size, NULL, 0);

                    pos += size;
                    break;
                case CHIP_FLASH_256KB:
                case CHIP_FLASH_512KB:
                case CHIP_FLASH_1MB:
                case CHIP_FLASH_8MB:
                case CHIP_FLASH_256KB_INFRARED:
                case CHIP_FLASH_512KB_INFRARED:
                case CHIP_FLASH_1MB_INFRARED:
                case CHIP_FLASH_8MB_INFRARED:
                    cmdSize = 4;
                    cmd[0] = SPI_FLASH_CMD_READ;
                    cmd[1] = (u8) (pos >> 16);
                    cmd[2] = (u8) (pos >> 8);
                    cmd[3] = (u8) pos;
                    res = spi_execute_command(chip, cmd, cmdSize, data, size, NULL, 0);

                    pos += size;
                    break;
                default:
                    res = R_APP_NOT_IMPLEMENTED;
                    break;
            }
        }

        if(R_SUCCEEDED(res) && bytesRead != NULL) {
            *bytesRead = pos - offset;
        }
    }

    return res;
}

static Result spi_write_data(SaveChip chip, u32* bytesWritten, void* data, u32 offset, u32 size) {
    Result res = 0;

    u32 pageSize = 0;
    u32 capacity = 0;
    if(R_SUCCEEDED(res = spi_get_page_size(chip, &pageSize)) && R_SUCCEEDED(res = spi_get_capacity(chip, &capacity))) {
        if(size > capacity - offset) {
            size = capacity - offset;
        }

        u32 pos = offset;
        if(size > 0 && R_SUCCEEDED(res = spi_wait_write_finish(chip))) {
            while(pos < offset + size) {
                u8 cmd[4] = {0};
                u32 cmdSize = 0;

                switch(chip) {
                    case CHIP_EEPROM_512B:
                        cmdSize = 2;
                        cmd[0] = (pos >= 0x100) ? (u8) SPI_EEPROM_512B_CMD_WRHI : (u8) SPI_EEPROM_512B_CMD_WRLO;
                        cmd[1] = (u8) pos;
                        break;
                    case CHIP_EEPROM_8KB:
                    case CHIP_EEPROM_64KB:
                        cmdSize = 3;
                        cmd[0] = SPI_EEPROM_CMD_WRITE;
                        cmd[1] = (u8) (pos >> 8);
                        cmd[2] = (u8) pos;
                        break;
                    case CHIP_EEPROM_128KB:
                        cmdSize = 4;
                        cmd[0] = SPI_EEPROM_CMD_WRITE;
                        cmd[1] = (u8) (pos >> 16);
                        cmd[2] = (u8) (pos >> 8);
                        cmd[3] = (u8) pos;
                        break;
                    case CHIP_FLASH_256KB:
                    case CHIP_FLASH_512KB:
                    case CHIP_FLASH_1MB:
                    case CHIP_FLASH_256KB_INFRARED:
                    case CHIP_FLASH_512KB_INFRARED:
                    case CHIP_FLASH_1MB_INFRARED:
                        cmdSize = 4;
                        cmd[0] = SPI_FLASH_CMD_PW;
                        cmd[1] = (u8) (pos >> 16);
                        cmd[2] = (u8) (pos >> 8);
                        cmd[3] = (u8) pos;
                        break;
                    case CHIP_FLASH_8MB:
                    case CHIP_FLASH_8MB_INFRARED:
                    default:
                        res = R_APP_NOT_IMPLEMENTED;
                        break;
                }

                if(R_FAILED(res)) {
                    break;
                }

                u32 pagePos = pos & ~(pageSize - 1);

                u32 currSize = size - (pos - offset);
                if(currSize > pageSize - (pos - pagePos)) {
                    currSize = pageSize - (pos - pagePos);
                }

                u8 ewCmd = SPI_CMD_WREN;
                if(R_SUCCEEDED(res = spi_execute_command(chip, &ewCmd, sizeof(ewCmd), NULL, 0, NULL, 0))) {
                    if(chip != CHIP_EEPROM_512B) {
                        ewCmd = SPI_CMD_RDSR;
                        u8 status = 0;
                        while(R_SUCCEEDED(res = spi_execute_command(chip, &ewCmd, sizeof(ewCmd), &status, sizeof(status), NULL, 0)) && (status & ~SPI_STAT_WEL));
                    }

                    if(R_SUCCEEDED(res) && R_SUCCEEDED(res = spi_execute_command(chip, cmd, cmdSize, NULL, 0, (u8*) data + (pos - offset), currSize))) {
                        res = spi_wait_write_finish(chip);
                    }
                }

                if(R_FAILED(res)) {
                    break;
                }

                pos = pagePos + pageSize;
            }
        }

        if(R_SUCCEEDED(res) && bytesWritten != NULL) {
            *bytesWritten = pos - offset;
        }
    }

    return res;
}

static Result spi_is_data_mirrored(SaveChip chip, u32 size, bool* mirrored) {
    Result res = 0;

    u8 original = 0;
    u8 oldMirror = 0;
    if(R_SUCCEEDED(res = spi_read_data(chip, NULL, &original, size - 1, sizeof(original)))
       && R_SUCCEEDED(res = spi_read_data(chip, NULL, &oldMirror, 2 * size - 1, sizeof(oldMirror)))) {
        u8 modified = ~original;
        u8 newMirror = 0;
        if(R_SUCCEEDED(res = spi_write_data(chip, NULL, &modified, size - 1, sizeof(modified)))
           && R_SUCCEEDED(res = spi_read_data(chip, NULL, &newMirror, 2 * size - 1, sizeof(newMirror)))
           && R_SUCCEEDED(res = spi_write_data(chip, NULL, &original, size - 1, sizeof(original)))) {
            if(mirrored != NULL) {
                *mirrored = oldMirror != newMirror;
            }
        }
    }

    return res;
}

static Result spi_get_save_chip(SaveChip* chip, SaveChip base) {
    Result res = 0;

    u32 jedecId = 0;
    u8 status = 0;
    if(R_SUCCEEDED(res = spi_read_jedec_id_status(base, &jedecId, &status))) {
        SaveChip c = CHIP_NONE;

        if(jedecId == 0xFFFFFF && ((status & 0xFD) == 0xF0 || (status & 0xFD) == 0x00)) {
            if((status & 0xFD) == 0xF0) {
                c = CHIP_EEPROM_512B;
            } else if((status & 0xFD) == 0x00) {
                bool mirrored = false;
                if(R_SUCCEEDED(res = spi_is_data_mirrored(CHIP_EEPROM_8KB, 8 * 1024, &mirrored))) {
                    if(mirrored) {
                        c = CHIP_EEPROM_8KB;
                    } else {
                        if(R_SUCCEEDED(res = spi_is_data_mirrored(CHIP_EEPROM_64KB, 64 * 1024, &mirrored))) {
                            if(mirrored) {
                                c = CHIP_EEPROM_64KB;
                            } else {
                                c = CHIP_EEPROM_128KB;
                            }
                        }
                    }
                }
            }
        } else {
            c = base < CHIP_FLASH_256KB_INFRARED ? CHIP_FLASH_256KB : CHIP_FLASH_256KB_INFRARED;

            switch(jedecId) {
                case 0x204012:
                case 0x621600:
                    c += CHIP_FLASH_256KB - CHIP_FLASH_256KB;
                    break;
                case 0x204013:
                case 0x621100:
                    c += CHIP_FLASH_512KB - CHIP_FLASH_256KB;
                    break;
                case 0x204014:
                    c += CHIP_FLASH_1MB - CHIP_FLASH_256KB;
                    break;
                case 0x202017:
                case 0x204017:
                    c += CHIP_FLASH_8MB - CHIP_FLASH_256KB;
                    break;
                default:
                    if(base < CHIP_FLASH_256KB_INFRARED) {
                        res = spi_get_save_chip(&c, CHIP_FLASH_256KB_INFRARED);
                    } else {
                        res = R_APP_NOT_IMPLEMENTED;
                    }

                    break;
            }
        }

        if(R_SUCCEEDED(res) && chip != NULL) {
            *chip = c;
        }
    }

    return res;
}

static SaveChip curr_chip = CHIP_NONE;

Result spi_init_card() {
    return spi_get_save_chip(&curr_chip, CHIP_EEPROM_512B);
}

Result spi_deinit_card() {
    curr_chip = CHIP_NONE;
    return 0;
}

Result spi_get_save_size(u32* size) {
    return spi_get_capacity(curr_chip, size);
}

Result spi_read_save(u32* bytesRead, void* data, u32 offset, u32 size) {
    return spi_read_data(curr_chip, bytesRead, data, offset, size);
}

Result spi_write_save(u32* bytesWritten, void* data, u32 offset, u32 size) {
    return spi_write_data(curr_chip, bytesWritten, data, offset, size);
}