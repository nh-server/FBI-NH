#pragma once

#include "../task/task.h"

void action_browse_boss_ext_save_data(ext_save_data_info* info, bool* populated);
void action_browse_user_ext_save_data(ext_save_data_info* , bool* populatedinfo);
void action_delete_ext_save_data(ext_save_data_info* info, bool* populated);

void action_browse_system_save_data(system_save_data_info* info, bool* populated);
void action_delete_system_save_data(system_save_data_info* info, bool* populated);

void action_install_cias(file_info* info, bool* populated);
void action_install_cias_delete(file_info* info, bool* populated);
void action_install_tickets(file_info* info, bool* populated);
void action_copy_contents(file_info* info, bool* populated);
void action_delete_contents(file_info* info, bool* populated);
void action_delete_dir(file_info* info, bool* populated);
void action_delete_dir_contents(file_info* info, bool* populated);
void action_delete_dir_cias(file_info* info, bool* populated);
void action_paste_contents(file_info* info, bool* populated);

void action_delete_pending_title(pending_title_info* info, bool* populated);
void action_delete_all_pending_titles(pending_title_info* info, bool* populated);

void action_delete_ticket(ticket_info* info, bool* populated);
void action_install_cdn(ticket_info* info, bool* populated);

void action_delete_title(title_info* info, bool* populated);
void action_launch_title(title_info* info, bool* populated);
void action_extract_smdh(title_info* info, bool* populated);
void action_browse_title_save_data(title_info* info, bool* populated);
void action_import_secure_value(title_info* info, bool* populated);
void action_export_secure_value(title_info* info, bool* populated);
void action_delete_secure_value(title_info* info, bool* populated);