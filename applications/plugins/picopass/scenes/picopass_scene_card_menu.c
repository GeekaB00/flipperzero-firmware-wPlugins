#include "../picopass_i.h"

enum SubmenuIndex {
    SubmenuIndexSave,
    SubmenuIndexSaveAsLF,
    SubmenuIndexResetiCE,
    SubmenuIndexResetiCL,
    SubmenuIndexResetiCS,
};

extern const uint8_t picopass_xice_key[];
extern const uint8_t picopass_xicl_key[];
extern const uint8_t picopass_xics_key[];

void picopass_scene_card_menu_submenu_callback(void* context, uint32_t index) {
    Picopass* picopass = context;

    view_dispatcher_send_custom_event(picopass->view_dispatcher, index);
}

void picopass_scene_card_menu_on_enter(void* context) {
    Picopass* picopass = context;
    Submenu* submenu = picopass->submenu;

    submenu_add_item(
        submenu, "Save", SubmenuIndexSave, picopass_scene_card_menu_submenu_callback, picopass);
    if(picopass->dev->dev_data.pacs.record.valid) {
        submenu_add_item(
            submenu,
            "Save as LF",
            SubmenuIndexSaveAsLF,
            picopass_scene_card_menu_submenu_callback,
            picopass);
    }
    submenu_add_item(submenu, "Reset iCE", SubmenuIndexResetiCE, picopass_scene_card_menu_submenu_callback, picopass);
    submenu_add_item(submenu, "Reset iCL", SubmenuIndexResetiCL, picopass_scene_card_menu_submenu_callback, picopass);
    submenu_add_item(submenu, "Reset iCS", SubmenuIndexResetiCS, picopass_scene_card_menu_submenu_callback, picopass);

    submenu_set_selected_item(
        picopass->submenu,
        scene_manager_get_scene_state(picopass->scene_manager, PicopassSceneCardMenu));

    view_dispatcher_switch_to_view(picopass->view_dispatcher, PicopassViewMenu);
}

bool picopass_scene_card_menu_on_event(void* context, SceneManagerEvent event) {
    Picopass* picopass = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == SubmenuIndexSave) {
            scene_manager_set_scene_state(
                picopass->scene_manager, PicopassSceneCardMenu, SubmenuIndexSave);
            scene_manager_next_scene(picopass->scene_manager, PicopassSceneSaveName);
            picopass->dev->format = PicopassDeviceSaveFormatHF;
            consumed = true;
        } else if(event.event == SubmenuIndexSaveAsLF) {
            scene_manager_set_scene_state(
                picopass->scene_manager, PicopassSceneCardMenu, SubmenuIndexSaveAsLF);
            picopass->dev->format = PicopassDeviceSaveFormatLF;
            scene_manager_next_scene(picopass->scene_manager, PicopassSceneSaveName);
            consumed = true;
        } else if(event.event == SubmenuIndexResetiCE) {
            scene_manager_set_scene_state(picopass->scene_manager, PicopassSceneCardMenu, SubmenuIndexResetiCE);
            memcpy(picopass->dev->dev_data.pacs.key, picopass_xice_key, PICOPASS_BLOCK_LEN);
            scene_manager_next_scene(picopass->scene_manager, PicopassSceneWriteKey);
            consumed = true;
        } else if(event.event == SubmenuIndexResetiCL) {
            scene_manager_set_scene_state(picopass->scene_manager, PicopassSceneCardMenu, SubmenuIndexResetiCE);
            memcpy(picopass->dev->dev_data.pacs.key, picopass_xicl_key, PICOPASS_BLOCK_LEN);
            scene_manager_next_scene(picopass->scene_manager, PicopassSceneWriteKey);
            consumed = true;
        } else if(event.event == SubmenuIndexResetiCS) {
            scene_manager_set_scene_state(picopass->scene_manager, PicopassSceneCardMenu, SubmenuIndexResetiCE);
            memcpy(picopass->dev->dev_data.pacs.key, picopass_xics_key, PICOPASS_BLOCK_LEN);
            scene_manager_next_scene(picopass->scene_manager, PicopassSceneWriteKey);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        consumed = scene_manager_search_and_switch_to_previous_scene(
            picopass->scene_manager, PicopassSceneStart);
    }

    return consumed;
}

void picopass_scene_card_menu_on_exit(void* context) {
    Picopass* picopass = context;

    submenu_reset(picopass->submenu);
}
