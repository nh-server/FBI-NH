#pragma once

#include "../task.h"

void action_browse_ext_save_data(ext_save_data_info* info);

void action_browse_system_save_data(system_save_data_info* info);

void action_install_cias_nand(file_info* info);
void action_install_cias_sd(file_info* info);
void action_copy_contents(file_info* info);
void action_delete_contents(file_info* info);
void action_delete_dir_contents(file_info* info);
void action_delete_dir_cias(file_info* info);
void action_paste_contents(file_info* info);

void action_delete_pending_title(pending_title_info* info);
void action_delete_all_pending_titles(pending_title_info* info);

void action_delete_ticket(ticket_info* info);

void action_delete_title(title_info* info);
void action_launch_title(title_info* info);
void action_browse_title_save_data(title_info* info);
void action_import_secure_value(title_info* info);
void action_export_secure_value(title_info* info);