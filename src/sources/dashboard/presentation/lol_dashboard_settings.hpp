#pragma once

#include "sources/dashboard/rendering/lol_visuals.hpp"

struct obs_data;

namespace sources {

lol_dashboard_font_style lol_dashboard_font_style_from_settings(obs_data *settings, const char *role);
lol_dashboard_regions lol_dashboard_regions_from_settings(obs_data *settings);
lol_dashboard_trail_filter lol_dashboard_trail_filter_from_settings(obs_data *settings);

} // namespace sources
