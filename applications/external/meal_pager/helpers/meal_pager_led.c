#include "meal_pager_led.h"

void meal_pager_blink_start_subghz(Meal_Pager* app) {
    furi_assert(app);
    notification_message(app->notification, &sequence_blink_stop);
    notification_message(app->notification, &sequence_blink_start_magenta);
}

void meal_pager_blink_start_compile(Meal_Pager* app) {
    furi_assert(app);
    notification_message(app->notification, &sequence_blink_stop);
    notification_message(app->notification, &sequence_blink_start_yellow);
}

void meal_pager_blink_stop(Meal_Pager* app) {
    furi_assert(app);
    notification_message(app->notification, &sequence_blink_stop);
}

void meal_pager_led_set_rgb(void* context, int red, int green, int blue) {
    Meal_Pager* app = context;
    if (app->led != 1) {
        return;
    }
    NotificationMessage notification_led_message_1;
    notification_led_message_1.type = NotificationMessageTypeLedRed;
    NotificationMessage notification_led_message_2;
    notification_led_message_2.type = NotificationMessageTypeLedGreen;
    NotificationMessage notification_led_message_3;
    notification_led_message_3.type = NotificationMessageTypeLedBlue;

    notification_led_message_1.data.led.value = red;
    notification_led_message_2.data.led.value = green;
    notification_led_message_3.data.led.value = blue;
    const NotificationSequence notification_sequence = {
        &notification_led_message_1,
        &notification_led_message_2,
        &notification_led_message_3,
        &message_do_not_reset,
        NULL,
    };
    notification_message(app->notification, &notification_sequence);
    furi_thread_flags_wait(0, FuriFlagWaitAny, 10); //Delay, prevent removal from RAM before LED value set    
}

void meal_pager_led_reset(void* context) {
    Meal_Pager* app = context;
    notification_message(app->notification, &sequence_reset_red);
    notification_message(app->notification, &sequence_reset_green);
    notification_message(app->notification, &sequence_reset_blue);
    
    furi_thread_flags_wait(0, FuriFlagWaitAny, 300); //Delay, prevent removal from RAM before LED value set    
}
