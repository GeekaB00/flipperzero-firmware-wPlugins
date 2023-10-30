#pragma once

#include "app_menu.h"
#include <gui/modules/submenu.h>
#include <gui/modules/widget.h>
#include <m-array.h>

ARRAY_DEF(ViewIdsArray, uint32_t, M_PTR_OPLIST);

struct AppMenu {
    ViewDispatcher* view_dispatcher;
    Submenu* submenu;
    ViewIdsArray_t view_ids;
};