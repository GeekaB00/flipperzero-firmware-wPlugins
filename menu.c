#include "fnaf.h"
#include "menu.h"

void draw_menu(Canvas* canvas, void* ctx) {
    Fnaf* fnaf = ctx;

    canvas_set_bitmap_mode(canvas, 1);
    canvas_draw_str(canvas, 15, 16, "New Game");
    uint8_t y = 32;
    if (fnaf->progress > 0) {
        char text[19];
        snprintf(text, 19, "Continue Night %u", fnaf->progress + 1);
        canvas_draw_str(canvas, 15, 32, text);
        y = 48;
    }
    canvas_draw_str(canvas, 16, y, "Exit");
    canvas_set_font(canvas, FontPrimary);

    switch (fnaf->menu_cursor) {
    case 0:
        canvas_draw_line(canvas, 14, 19, 69, 19);
        break;
    case 1:
        if (fnaf->progress > 0) { canvas_draw_line(canvas, 14, 35, 108, 35); } else
            canvas_draw_line(canvas, 14, 35, 35, 35);
        break;
    case 2:
        canvas_draw_line(canvas, 14, 51, 35, 51);
        break;
    }
}

bool menu_input(void* ctx) {
    Fnaf* fnaf = ctx;
    if (fnaf->event.type == InputTypePress) {
        switch (fnaf->event.key) {
        case InputKeyLeft:
            break;
        case InputKeyRight:

            // REMOVE FOR RELEASE
            fnaf->progress += 1;
            // REMOVE FOR RELEASE

            if (fnaf->progress > 6) fnaf->progress = 0;
            break;
        case InputKeyUp:
            fnaf->menu_cursor -= 1;
            break;
        case InputKeyDown:
            fnaf->menu_cursor += 1;
            break;
        case InputKeyOk:
            switch (fnaf->menu_cursor) {
            case 0:
                // Do thing
                break;
            case 1:
                // Do thing
                if (fnaf->progress == 0) return 0;
                break;
            case 2:
                return false;
                break;
            }
            break;
        case InputKeyBack:
            return false;
        default:
            break;
        }
        if (fnaf->menu_cursor > 2) {
            fnaf->menu_cursor = 0;
        } else if (fnaf->menu_cursor < 0) {
            fnaf->menu_cursor = 2;
        }
        if (fnaf->menu_cursor > 1 && fnaf->progress == 0) fnaf->menu_cursor = 0;
    }
    return true;
}
