#pragma once

#include "../ui.h"

void dump_nand();
void extsavedata_open();
void files_open(FS_Archive archive);
void files_open_sd();
void files_open_ctr_nand();
void files_open_twl_nand();
void networkinstall_open(FS_MediaType dest);
void networkinstall_open_sd();
void networkinstall_open_nand();
void pendingtitles_open();
void systemsavedata_open();
void tickets_open();
void titles_open();