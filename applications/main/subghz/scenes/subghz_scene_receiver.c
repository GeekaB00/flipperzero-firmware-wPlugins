#include "../subghz_i.h"
#include <dolphin/dolphin.h>
#include <lib/subghz/protocols/bin_raw.h>

#define TAG "SubGhzSceneReceiver"

const NotificationSequence subghz_sequence_beep = {
    &message_vibro_on,
    &message_note_c6,
    &message_delay_50,
    &message_sound_off,
    &message_vibro_off,
    NULL,
};

const NotificationSequence subghz_sequence_rx = {
    &message_green_255,

    &message_display_backlight_on,

    &message_vibro_on,
    &message_note_c6,
    &message_delay_50,
    &message_sound_off,
    &message_vibro_off,

    &message_delay_50,
    NULL,
};

const NotificationSequence subghz_sequence_repeat = {
    &message_red_255,
    //&message_display_backlight_on,
    &message_vibro_on,
    &message_note_c6,
    &message_delay_50,
    &message_sound_off,
    &message_vibro_off,
    NULL,
};

const NotificationSequence subghz_sequence_rx_locked = {
    &message_green_255,

    &message_display_backlight_on,

    &message_vibro_on,
    &message_note_c6,
    &message_delay_50,
    &message_sound_off,
    &message_vibro_off,

    &message_delay_500,

    &message_display_backlight_off,
    NULL,
};

static void subghz_scene_receiver_update_statusbar(void* context) {
    SubGhz* subghz = context;
    FuriString* history_stat_str = furi_string_alloc();
    if(!subghz_history_get_text_space_left(subghz->history, history_stat_str)) {
        FuriString* frequency_str = furi_string_alloc();
        FuriString* modulation_str = furi_string_alloc();

#ifdef SUBGHZ_EXT_PRESET_NAME
        if(subghz_history_get_last_index(subghz->history) > 0) {
            subghz_txrx_get_frequency_and_modulation(
                subghz->txrx, frequency_str, modulation_str, false);
        } else {
            FuriString* temp_str = furi_string_alloc();

            subghz_txrx_get_frequency_and_modulation(subghz->txrx, frequency_str, temp_str, true);
            furi_string_printf(
                modulation_str,
                "%s        Mod: %s",
                (subghz_txrx_radio_device_get(subghz->txrx) == SubGhzRadioDeviceTypeInternal) ?
                    "Int" :
                    "Ext",
                furi_string_get_cstr(temp_str));
            furi_string_free(temp_str);
        }
#else
        subghz_txrx_get_frequency_and_modulation(
            subghz->txrx, frequency_str, modulation_str, false);
#endif

        subghz_view_receiver_add_data_statusbar(
            subghz->subghz_receiver,
            furi_string_get_cstr(frequency_str),
            furi_string_get_cstr(modulation_str),
            furi_string_get_cstr(history_stat_str),
            subghz_txrx_hopper_get_state(subghz->txrx) != SubGhzHopperStateOFF,
            READ_BIT(subghz->filter, SubGhzProtocolFlag_BinRAW) > 0,
            (subghz->repeater != SubGhzRepeaterOff));

        furi_string_free(frequency_str);
        furi_string_free(modulation_str);
    } else {
        subghz_view_receiver_add_data_statusbar(
            subghz->subghz_receiver,
            furi_string_get_cstr(history_stat_str),
            "",
            "",
            subghz_txrx_hopper_get_state(subghz->txrx) != SubGhzHopperStateOFF,
            READ_BIT(subghz->filter, SubGhzProtocolFlag_BinRAW) > 0,
            (subghz->repeater != SubGhzRepeaterOff));
        subghz->state_notifications = SubGhzNotificationStateIDLE;
    }
    furi_string_free(history_stat_str);

    subghz_view_receiver_set_radio_device_type(
        subghz->subghz_receiver, subghz_txrx_radio_device_get(subghz->txrx));
}

void subghz_scene_receiver_callback(SubGhzCustomEvent event, void* context) {
    furi_assert(context);
    SubGhz* subghz = context;
    view_dispatcher_send_custom_event(subghz->view_dispatcher, event);
}

static void subghz_scene_add_to_history_callback(
    SubGhzReceiver* receiver,
    SubGhzProtocolDecoderBase* decoder_base,
    void* context) {
    furi_assert(context);
    SubGhz* subghz = context;

    // The check can be moved to /lib/subghz/receiver.c, but may result in false positives
    if((decoder_base->protocol->flag & subghz->ignore_filter) == 0) {
        SubGhzHistory* history = subghz->history;
        FuriString* item_name = furi_string_alloc();
        FuriString* item_time = furi_string_alloc();
        uint16_t idx = subghz_history_get_item(history);

        //Need the preset for the repeater, move her up here!
        SubGhzRadioPreset preset = subghz_txrx_get_preset(subghz->txrx);

        /* This is where the repeater logic ended up going!
           Instead of adding to the list, Ill beep and send the key on!
           Bit of a Hack, but thats the flipper code for you!
        */

        if(subghz->repeater != SubGhzRepeaterOff) {
            //Start TX Counter, at the moment I just send the key as fixed length.
            //Next version will look at Te time in BinRAW (since its locked to that now!)
            //and transmit for the same time as receive was. It works well for my purposes as is though!

            //Stop the Radio from Receiving.
            subghz_txrx_hopper_pause(subghz->txrx);
            subghz_receiver_reset(receiver);

            //Get the data to send.
            subghz_protocol_decoder_base_serialize(decoder_base, subghz->repeater_tx, &preset);

            uint32_t tmpTe = 300;
            if(!flipper_format_rewind(subghz->repeater_tx)) {
                FURI_LOG_E(TAG, "Rewind error");
                return;
            }
            if(!flipper_format_read_uint32(subghz->repeater_tx, "TE", (uint32_t*)&tmpTe, 1)) {
                FURI_LOG_E(TAG, "Missing TE");
                return;
            } else {
                //Save our TX variables now, start TX on the next tick event so the we arent tangled with the worker.
                subghz->RepeaterTXLength = tmpTe;
                subghz->state_notifications = SubGhzNotificationStateTx;
                notification_message(subghz->notifications, &subghz_sequence_repeat);
                FURI_LOG_I(TAG, "Key Received, Transmitting now.");
            }
        } else {
            if(subghz_history_add_to_history(history, decoder_base, &preset)) {
                furi_string_reset(item_name);
                furi_string_reset(item_time);

                subghz->state_notifications = SubGhzNotificationStateRxDone;

                subghz_history_get_text_item_menu(history, item_name, idx);
                subghz_history_get_time_item_menu(history, item_time, idx);
                subghz_view_receiver_add_item_to_menu(
                    subghz->subghz_receiver,
                    furi_string_get_cstr(item_name),
                    furi_string_get_cstr(item_time),
                    subghz_history_get_type_protocol(history, idx));

                subghz_scene_receiver_update_statusbar(subghz);
                if(subghz_history_get_text_space_left(subghz->history, NULL)) {
                    notification_message(subghz->notifications, &sequence_error);
                }
            }
            subghz_receiver_reset(receiver);
            subghz_rx_key_state_set(subghz, SubGhzRxKeyStateAddKey);
        }
        furi_string_free(item_name);
        furi_string_free(item_time);

    } else {
        FURI_LOG_I(TAG, "%s protocol ignored", decoder_base->protocol->name);
    }
}

void subghz_scene_receiver_on_enter(void* context) {
    SubGhz* subghz = context;
    SubGhzHistory* history = subghz->history;

    FuriString* item_name = furi_string_alloc();
    FuriString* item_time = furi_string_alloc();
    subghz->repeater_tx = flipper_format_string_alloc();
    subghz->RepeaterTXLength = 0;

    if(subghz_rx_key_state_get(subghz) == SubGhzRxKeyStateIDLE) {
#if SUBGHZ_LAST_SETTING_SAVE_PRESET
        subghz_txrx_set_preset_internal(
            subghz->txrx, subghz->last_settings->frequency, subghz->last_settings->preset_index);
#else
        subghz_txrx_set_default_preset(subghz->txrx, subghz->last_settings->frequency);
#endif

        subghz->filter = subghz->last_settings->filter;
        subghz_txrx_receiver_set_filter(subghz->txrx, subghz->filter);
        subghz->ignore_filter = subghz->last_settings->ignore_filter;

        subghz_history_reset(history);
        subghz_rx_key_state_set(subghz, SubGhzRxKeyStateStart);
        subghz->idx_menu_chosen = 0;
    }

    subghz_view_receiver_set_lock(subghz->subghz_receiver, subghz_is_locked(subghz));
    subghz_view_receiver_set_mode(subghz->subghz_receiver, SubGhzViewReceiverModeLive);

    // Load history to receiver
    subghz_view_receiver_exit(subghz->subghz_receiver);
    for(uint16_t i = 0; i < subghz_history_get_item(history); i++) {
        furi_string_reset(item_name);
        furi_string_reset(item_time);
        subghz_history_get_text_item_menu(history, item_name, i);
        subghz_history_get_time_item_menu(history, item_time, i);
        subghz_view_receiver_add_item_to_menu(
            subghz->subghz_receiver,
            furi_string_get_cstr(item_name),
            furi_string_get_cstr(item_time),
            subghz_history_get_type_protocol(history, i));
        subghz_rx_key_state_set(subghz, SubGhzRxKeyStateAddKey);
    }
    furi_string_free(item_name);
    furi_string_free(item_time);
    subghz_view_receiver_set_callback(
        subghz->subghz_receiver, subghz_scene_receiver_callback, subghz);
    subghz_txrx_set_rx_callback(subghz->txrx, subghz_scene_add_to_history_callback, subghz);

    if(!subghz_history_get_text_space_left(subghz->history, NULL)) {
        subghz->state_notifications = SubGhzNotificationStateRx;
    }

    // Check if hopping was enabled, and restart the radio.
    subghz_txrx_hopper_set_state(
        subghz->txrx,
        subghz->last_settings->enable_hopping ? SubGhzHopperStateRunning : SubGhzHopperStateOFF);

    // Check if Sound was enabled, and restart the Speaker.
    subghz_txrx_speaker_set_state(
        subghz->txrx,
        subghz->last_settings->enable_sound ? SubGhzSpeakerStateEnable :
                                              SubGhzSpeakerStateDisable);

    /* Proper fix now! Start the radio again. */
    subghz_txrx_rx_start(subghz->txrx);
    subghz_view_receiver_set_idx_menu(subghz->subghz_receiver, subghz->idx_menu_chosen);
    subghz_txrx_hopper_unpause(subghz->txrx);

    //to use a universal decoder, we are looking for a link to it
    furi_check(
        subghz_txrx_load_decoder_by_name_protocol(subghz->txrx, SUBGHZ_PROTOCOL_BIN_RAW_NAME));

    //Remember if the repeater was loaded, need to set protocol back to BinRAW if we are repeating.
    //Do this last so that I can override the BinRAW load or not!
    if(subghz->last_settings->RepeaterState != SubGhzRepeaterOff) {
        subghz->repeater = subghz->last_settings->RepeaterState;

        //User had BinRAW on if the last settings had BinRAW on, if not, repeater is on, and BinRAW goes on, but State CHanged is false!
        subghz->BINRawStateChanged =
            (subghz->last_settings->filter !=
             (SubGhzProtocolFlag_Decodable | SubGhzProtocolFlag_BinRAW));
        subghz->filter = SubGhzProtocolFlag_Decodable | SubGhzProtocolFlag_BinRAW;
        subghz_txrx_receiver_set_filter(subghz->txrx, subghz->filter);
    } else {
        subghz->repeater = SubGhzRepeaterOff;
        subghz->BINRawStateChanged = false;
    }

    subghz_scene_receiver_update_statusbar(subghz);

    view_dispatcher_switch_to_view(subghz->view_dispatcher, SubGhzViewIdReceiver);
}

bool subghz_scene_receiver_on_event(void* context, SceneManagerEvent event) {
    SubGhz* subghz = context;
    bool consumed = false;
    if(event.type == SceneManagerEventTypeCustom) {
        // Save cursor position before going to any other dialog
        subghz->idx_menu_chosen = subghz_view_receiver_get_idx_menu(subghz->subghz_receiver);

        switch(event.event) {
        case SubGhzCustomEventViewReceiverBack:
            // Stop CC1101 Rx
            subghz->state_notifications = SubGhzNotificationStateIDLE;
            subghz_txrx_stop(subghz->txrx);
            subghz_txrx_hopper_set_state(subghz->txrx, SubGhzHopperStateOFF);
            subghz_txrx_set_rx_callback(subghz->txrx, NULL, subghz);

            if(subghz_rx_key_state_get(subghz) == SubGhzRxKeyStateAddKey) {
                subghz_rx_key_state_set(subghz, SubGhzRxKeyStateExit);
                scene_manager_next_scene(subghz->scene_manager, SubGhzSceneNeedSaving);
            } else {
                subghz_rx_key_state_set(subghz, SubGhzRxKeyStateIDLE);
                subghz_txrx_set_default_preset(subghz->txrx, subghz->last_settings->frequency);

                //The user could have come from the Start Scene, or birect through Send RAW only then Listen TX.
                //Go back to the right scene!

                //Did the user come from the Start Menu or Browser loading a Sub file?
                if(scene_manager_has_previous_scene(subghz->scene_manager, SubGhzSceneStart)) {
                    //Scene exists, go back to it
                    scene_manager_search_and_switch_to_previous_scene(
                        subghz->scene_manager, SubGhzSceneStart);
                } else {
                    //We came through transmitter or Send RAW scenes. go back to them!
                    //if(subghz_get_load_type_file(subghz) == SubGhzLoadTypeFileRaw) {
                    //Load Raw TX
                    //    subghz_rx_key_state_set(subghz, SubGhzRxKeyStateRAWLoad);
                    if(subghz_key_load(subghz, furi_string_get_cstr(subghz->file_path), false)) {
                        scene_manager_previous_scene(subghz->scene_manager);
                    } else {
                        //Go to Start Scene!
                        scene_manager_next_scene(subghz->scene_manager, SubGhzSceneStart);
                    }
                }

                //scene_manager_search_and_switch_to_previous_scene(subghz->scene_manager, SubGhzSceneStart);
            }
            consumed = true;
            break;
        case SubGhzCustomEventViewReceiverOK:
            //I am turning off the Radio, if the FLipper people want to fix the bug they can!
            subghz->state_notifications = SubGhzNotificationStateIDLE;
            subghz_txrx_stop(subghz->txrx);
            subghz_txrx_hopper_pause(subghz->txrx);

            // Show file info, scene: receiver_info
            scene_manager_next_scene(subghz->scene_manager, SubGhzSceneReceiverInfo);
            dolphin_deed(DolphinDeedSubGhzReceiverInfo);
            consumed = true;
            break;
        case SubGhzCustomEventViewReceiverOKLong:
            //CC1101 Stop RX
            subghz_txrx_stop(subghz->txrx);
            subghz_txrx_hopper_pause(subghz->txrx);

            //Start TX
            if(!subghz_tx_start(
                   subghz,
                   subghz_history_get_raw_data(subghz->history, subghz->idx_menu_chosen))) {
                subghz_txrx_rx_start(subghz->txrx);
                subghz_txrx_hopper_unpause(subghz->txrx);
                subghz->state_notifications = SubGhzNotificationStateRx;
            } else {
                notification_message(subghz->notifications, &subghz_sequence_beep);
                subghz->state_notifications = SubGhzNotificationStateTx;
            }
            consumed = true;
            break;
        case SubGhzCustomEventViewReceiverOKRelease:
            if(subghz->state_notifications == SubGhzNotificationStateTx) {
                //CC1101 Stop Tx -> Start RX
                subghz->state_notifications = SubGhzNotificationStateIDLE;
                subghz_txrx_stop(subghz->txrx);
                subghz_txrx_rx_start(subghz->txrx);
                subghz_txrx_hopper_unpause(subghz->txrx);
                if(!subghz_history_get_text_space_left(subghz->history, NULL)) {
                    subghz->state_notifications = SubGhzNotificationStateRx;
                }
                consumed = true;
                break;
            }
        case SubGhzCustomEventViewReceiverDeleteItem:
            subghz->state_notifications = SubGhzNotificationStateRx;

            subghz_view_receiver_disable_draw_callback(subghz->subghz_receiver);

            subghz_history_delete_item(subghz->history, subghz->idx_menu_chosen);
            subghz_view_receiver_delete_element_callback(subghz->subghz_receiver);
            subghz_view_receiver_enable_draw_callback(subghz->subghz_receiver);

            subghz_scene_receiver_update_statusbar(subghz);
            subghz->idx_menu_chosen = subghz_view_receiver_get_idx_menu(subghz->subghz_receiver);
            if(subghz_history_get_last_index(subghz->history) == 0) {
                subghz_rx_key_state_set(subghz, SubGhzRxKeyStateStart);
            }
            consumed = true;
            break;
        case SubGhzCustomEventViewReceiverConfig:

            // Actually signals are received but SubGhzNotificationStateRx is not working inside Config Scene

            /* ^^^ This is the comment that started all this crap for me! No, signals are not received, 
                the decode cant send to the history in the background. Info has to be active scene. 
            T   his could be changed, but for now lets just make the radio behave as the interface does!

                Making the worker call subghz_scene_add_to_history_callback and making it work properly probably is best in long run.
            */

            //I am turning off the Radio, if the FLipper people want to fix the bug they can!
            subghz->state_notifications = SubGhzNotificationStateIDLE;
            subghz_txrx_stop(subghz->txrx);
            subghz_txrx_hopper_pause(subghz->txrx);

            //Start the config scene.
            scene_manager_set_scene_state(
                subghz->scene_manager, SubGhzViewIdReceiver, SubGhzCustomEventManagerSet);
            scene_manager_next_scene(subghz->scene_manager, SubGhzSceneReceiverConfig);
            consumed = true;
            break;
        case SubGhzCustomEventViewReceiverOffDisplay:
            notification_message(subghz->notifications, &sequence_display_backlight_off);
            consumed = true;
            break;
        case SubGhzCustomEventViewReceiverUnlock:
            subghz_unlock(subghz);
            consumed = true;
            break;
        default:
            break;
        }
    } else if(event.type == SceneManagerEventTypeTick) {
        if(subghz->state_notifications != SubGhzNotificationStateTx) {
            //Do the receive stuff, the repeater TX logic is here now!
            if(subghz_txrx_hopper_get_state(subghz->txrx) != SubGhzHopperStateOFF) {
                subghz_txrx_hopper_update(subghz->txrx);
                subghz_scene_receiver_update_statusbar(subghz);
            }

            SubGhzThresholdRssiData ret_rssi = subghz_threshold_get_rssi_data(
                subghz->threshold_rssi, subghz_txrx_radio_device_get_rssi(subghz->txrx));

            subghz_receiver_rssi(subghz->subghz_receiver, ret_rssi.rssi);
            subghz_protocol_decoder_bin_raw_data_input_rssi(
                (SubGhzProtocolDecoderBinRAW*)subghz_txrx_get_decoder(subghz->txrx),
                ret_rssi.rssi);
        } else {
            //Start or Stop TX?
            if(subghz->RepeaterStartTime == 0) {
                subghz->RepeaterStartTime = furi_get_tick();

                //CC1101 Stop RX -> Start TX
                if(!subghz_tx_start(subghz, subghz->repeater_tx)) {
                    subghz_txrx_rx_start(subghz->txrx);
                    subghz_txrx_hopper_unpause(subghz->txrx);
                    subghz->state_notifications = SubGhzNotificationStateRx;
                }
            } else {
                uint32_t tmp = furi_get_tick() - subghz->RepeaterStartTime;
                uint8_t RepeatMultiplier = (subghz->repeater == SubGhzRepeaterOnShort) ? 1 :        //No repeats, 1 key Tx
                                       (subghz->repeater == SubGhzRepeaterOnLong)  ? 7 :        //Long Repeat
                                                                                     3;         //Normal Repeat
                if(tmp > furi_ms_to_ticks(subghz->RepeaterTXLength) * RepeatMultiplier) {
                    /* AAAAARGH! The FLipper cant tell me how long the receive was happening.
                   I can find the minimum time to transmit a key though, so Ive just doubled it to get the key
                   to send OK to a receiver. It works on my car, by who knows how it will work on devices that look at TX time1
                   At least the key is guaranteed to be transmitted up to TWICE! Regardless of Te of a Key
                    
                    This is the best repeaterv the flipper can do without diving deeper into the firmware for some big changes!
                */
                    FURI_LOG_I(TAG, "TXLength: %lu TxTime: %lu", subghz->RepeaterTXLength, tmp);

                    subghz_txrx_stop(subghz->txrx);
                    subghz->RepeaterTXLength = 0;
                    subghz->RepeaterStartTime = 0;
                    subghz->ignore_filter = 0x00;
                    subghz_txrx_rx_start(subghz->txrx);
                    subghz_txrx_hopper_unpause(subghz->txrx);
                    subghz->state_notifications = SubGhzNotificationStateRx;
                    //notification_message(subghz->notifications, &subghz_sequence_repeat);
                }
            }
        }
    }

    switch(subghz->state_notifications) {
    case SubGhzNotificationStateRx:
        notification_message(subghz->notifications, &sequence_blink_cyan_10);
        break;
    case SubGhzNotificationStateRxDone:
        if(!subghz_is_locked(subghz)) {
            notification_message(subghz->notifications, &subghz_sequence_rx);
        } else {
            notification_message(subghz->notifications, &subghz_sequence_rx_locked);
        }
        subghz->state_notifications = SubGhzNotificationStateRx;
        break;
    case SubGhzNotificationStateTx:
        notification_message(subghz->notifications, &sequence_blink_red_10);
        break;
    default:
        break;
    }

    return consumed;
}

void subghz_scene_receiver_on_exit(void* context) {
    SubGhz* subghz = context;
    flipper_format_free(subghz->repeater_tx);

    //UNUSED(context);
}
