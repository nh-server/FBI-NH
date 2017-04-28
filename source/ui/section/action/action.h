#pragma once

typedef struct ticket_info_s ticket_info;
typedef struct linked_list_s linked_list;
typedef struct list_item_s list_item;
typedef struct ui_view_s ui_view;

#define INSTALL_URL_MAX 1024
#define INSTALL_URLS_MAX 128

void action_browse_boss_ext_save_data(linked_list* items, list_item* selected);
void action_browse_user_ext_save_data(linked_list* items, list_item* selected);
void action_delete_ext_save_data(linked_list* items, list_item* selected);

void action_browse_system_save_data(linked_list* items, list_item* selected);
void action_delete_system_save_data(linked_list* items, list_item* selected);

void action_install_cia(linked_list* items, list_item* selected);
void action_install_cia_delete(linked_list* items, list_item* selected);
void action_install_cias(linked_list* items, list_item* selected);
void action_install_cias_delete(linked_list* items, list_item* selected);
void action_install_ticket(linked_list* items, list_item* selected);
void action_install_ticket_delete(linked_list* items, list_item* selected);
void action_install_tickets(linked_list* items, list_item* selected);
void action_install_tickets_delete(linked_list* items, list_item* selected);
void action_delete_file(linked_list* items, list_item* selected);
void action_delete_dir(linked_list* items, list_item* selected);
void action_delete_dir_contents(linked_list* items, list_item* selected);
void action_delete_dir_cias(linked_list* items, list_item* selected);
void action_delete_dir_tickets(linked_list* items, list_item* selected);
void action_new_folder(linked_list* items, list_item* selected);
void action_paste_contents(linked_list* items, list_item* selected);
void action_rename(linked_list* items, list_item* selected);

void action_delete_pending_title(linked_list* items, list_item* selected);
void action_delete_all_pending_titles(linked_list* items, list_item* selected);

void action_delete_ticket(linked_list* items, list_item* selected);
void action_delete_tickets_unused(linked_list* items, list_item* selected);
void action_install_cdn(linked_list* items, list_item* selected);
void action_install_cdn_noprompt(volatile bool* done, ticket_info* info, bool finishedPrompt, bool promptVersion);

void action_delete_title(linked_list* items, list_item* selected);
void action_delete_title_ticket(linked_list* items, list_item* selected);
void action_launch_title(linked_list* items, list_item* selected);
void action_extract_smdh(linked_list* items, list_item* selected);
void action_import_seed(linked_list* items, list_item* selected);
void action_erase_twl_save(linked_list* items, list_item* selected);
void action_export_twl_save(linked_list* items, list_item* selected);
void action_import_twl_save(linked_list* items, list_item* selected);
void action_browse_title_save_data(linked_list* items, list_item* selected);
void action_import_secure_value(linked_list* items, list_item* selected);
void action_export_secure_value(linked_list* items, list_item* selected);
void action_delete_secure_value(linked_list* items, list_item* selected);

void action_install_url(const char* confirmMessage, const char* urls, void* userData, void (*finished)(void* data),
                                                                                      void (*drawTop)(ui_view* view, void* data, float x1, float y1, float x2, float y2, u32 index));

void action_install_titledb(linked_list* items, list_item* selected);
void action_update_titledb(linked_list* items, list_item* selected);