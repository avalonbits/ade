/*
 * Copyright (C) 2026  Igor Cananea <icc@avalonbits.com>
 * Author: Igor Cananea <icc@avalonbits.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "options.h"

#include "app.h"
#include "settings.h"

// A fresh ade.ini: the editor's settings, naming ade, with the example font
// among the fonts ade shares with aed.
static int render(const void* values, char* buf, int max) {
    return ed_settings_render(values, "ade", app_get()->font_dir, buf, max);
}

static const cfg_schema ADE_CONFIG = { ED_SETTINGS, ED_SETTINGS_COUNT, render };

const char* ade_settings(editor* ed) {
    return ed_settings_apply(ed, &ADE_CONFIG);
}

void ade_cmd_settings(editor* ed) {
    ed_cmd_settings(ed, &ADE_CONFIG);
}
