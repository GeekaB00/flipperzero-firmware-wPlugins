#include "../meal_pager_i.h"
#include "../helpers/meal_pager_custom_event.h"
#include "../helpers/retekess/meal_pager_retekess_t119.h"
#include "../views/meal_pager_transmit.h"
#include "../helpers/meal_pager_led.h"
#include "../helpers/subghz/subghz.h"

void meal_pager_transmit_callback(Meal_PagerCustomEvent event, void* context) {
    furi_assert(context);
    Meal_Pager* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, event);
}

void meal_pager_scene_transmit_on_enter(void* context) {
    furi_assert(context);
    Meal_Pager* app = context;
    FURI_LOG_D(TAG, "Type is %lu", app->pager_type);

    meal_pager_blink_start_compile(app);
    meal_pager_transmit_model_set_type(app->meal_pager_transmit, app->pager_type);
    meal_pager_transmit_model_set_station(app->meal_pager_transmit, app->current_station);
    meal_pager_transmit_model_set_pager(app->meal_pager_transmit, app->current_pager);
    meal_pager_transmit_set_callback(app->meal_pager_transmit, meal_pager_transmit_callback, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, Meal_PagerViewIdTransmit);
    meal_pager_retekess_t119_generate_all(app);

    meal_pager_blink_start_subghz(app);
    app->stop_transmit = false;
    FURI_LOG_D(TAG, "Generated tmp.sub");
    subghz_send(app);
    meal_pager_blink_stop(app);
}

bool meal_pager_scene_transmit_on_event(void* context, SceneManagerEvent event) {
    Meal_Pager* app = context;
    bool consumed = false;
    
    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
            case Meal_PagerCustomEventTransmitLeft:
            case Meal_PagerCustomEventTransmitRight:
                break;
            case Meal_PagerCustomEventTransmitUp:
            case Meal_PagerCustomEventTransmitDown:
                break;
            case Meal_PagerCustomEventTransmitBack:
                notification_message(app->notification, &sequence_reset_red);
                notification_message(app->notification, &sequence_reset_green);
                notification_message(app->notification, &sequence_reset_blue);
                app->stop_transmit = true;
                if(!scene_manager_search_and_switch_to_previous_scene(
                    app->scene_manager, Meal_PagerSceneMenu)) {
                        scene_manager_stop(app->scene_manager);
                        view_dispatcher_stop(app->view_dispatcher);
                    }
                consumed = true;
                break;
        }
    }
    
    return consumed;
}

void meal_pager_scene_transmit_on_exit(void* context) {
    Meal_Pager* app = context;
    UNUSED(app);
}