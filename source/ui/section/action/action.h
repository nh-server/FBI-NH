#pragma once

typedef struct file_info_s file_info;
typedef struct linked_list_s linked_list;
typedef struct list_item_s list_item;

void action_browse_boss_ext_save_data(linked_list* items, list_item* selected);
void action_browse_user_ext_save_data(linked_list* items, list_item* selected);
void action_delete_ext_save_data(linked_list* items, list_item* selected);

void action_browse_system_save_data(linked_list* items, list_item* selected);
void action_delete_system_save_data(linked_list* items, list_item* selected);

void action_install_cia(linked_list* items, list_item* selected, file_info* target);
void action_install_cia_delete(linked_list* items, list_item* selected, file_info* target);
void action_install_cias(linked_list* items, list_item* selected, file_info* target);
void action_install_cias_delete(linked_list* items, list_item* selected, file_info* target);
void action_install_ticket(linked_list* items, list_item* selected, file_info* target);
void action_install_ticket_delete(linked_list* items, list_item* selected, file_info* target);
void action_install_tickets(linked_list* items, list_item* selected, file_info* target);
void action_install_tickets_delete(linked_list* items, list_item* selected, file_info* target);
void action_copy_content(linked_list* items, list_item* selected, file_info* target);
void action_copy_contents(linked_list* items, list_item* selected, file_info* target);
void action_delete_contents(linked_list* items, list_item* selected, file_info* target);
void action_delete_dir(linked_list* items, list_item* selected, file_info* target);
void action_delete_dir_contents(linked_list* items, list_item* selected, file_info* target);
void action_delete_dir_cias(linked_list* items, list_item* selected, file_info* target);
void action_delete_dir_tickets(linked_list* items, list_item* selected, file_info* target);
void action_paste_contents(linked_list* items, list_item* selected, file_info* target);

void action_delete_pending_title(linked_list* items, list_item* selected);
void action_delete_all_pending_titles(linked_list* items, list_item* selected);

void action_delete_ticket(linked_list* items, list_item* selected);
void action_install_cdn(linked_list* items, list_item* selected);

void action_delete_title(linked_list* items, list_item* selected);
void action_launch_title(linked_list* items, list_item* selected);
void action_extract_smdh(linked_list* items, list_item* selected);
void action_browse_title_save_data(linked_list* items, list_item* selected);
void action_import_secure_value(linked_list* items, list_item* selected);
void action_export_secure_value(linked_list* items, list_item* selected);
void action_delete_secure_value(linked_list* items, list_item* selected);