#include "../../lightmeter.h"

void lightmeter_scene_help_on_enter(void* context) {
    LightMeterApp* lightmeter = context;

    FuriString* temp_str;
    temp_str = furi_string_alloc();
    furi_string_printf(
        temp_str,
        "App works with BH1750\nambient light sensor\nconnected via I2C interface\n\n");
    furi_string_cat(
        temp_str,
        "\e#Pinout:\r\n");
    furi_string_cat(
        temp_str,
        "    SDA: 15 [C1]\r\n"
        "    SCL: 16 [C0]\r\n");

    widget_add_text_scroll_element(lightmeter->widget, 0, 0, 128, 64, furi_string_get_cstr(temp_str));
    furi_string_free(temp_str);

    view_dispatcher_switch_to_view(lightmeter->view_dispatcher, LightMeterAppViewHelp);
}

bool lightmeter_scene_help_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void lightmeter_scene_help_on_exit(void* context) {
    LightMeterApp* lightmeter = context;

    widget_reset(lightmeter->widget);
}
