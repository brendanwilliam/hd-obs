#include "sources/dashboard/presentation/lol_dashboard_settings.hpp"

#include <QString>
#include <algorithm>
#include <obs-module.h>

namespace sources {
namespace {
lol_dashboard_regions::widget widget_from_settings(obs_data_t *settings,
						   const std::string &key)
{
	return static_cast<lol_dashboard_regions::widget>(
		std::clamp(int(obs_data_get_int(settings, key.c_str())), 0, 6));
}
lol_dashboard_regions::section
section_from_settings(obs_data_t *settings, const char *name,
		      lol_dashboard_regions::section fallback)
{
	const std::string prefix = std::string("lol_dashboard.hud.") + name;
	fallback.enabled = obs_data_get_bool(settings, (prefix + ".enabled").c_str());
	fallback.count = std::clamp(
		int(obs_data_get_int(settings, (prefix + ".count").c_str())), 1, 4);
	for (int index = 0; index < fallback.count; ++index)
		fallback.widgets[index] = widget_from_settings(
			settings, prefix + ".slot_" + std::to_string(index + 1));
	for (int index = 0; index < fallback.count; ++index) {
		fallback.intensity_metrics[index] = std::clamp(
			int(obs_data_get_int(settings, (prefix + ".slot_" +
							std::to_string(index + 1) +
							".intensity_metric")
							       .c_str())),
			0, 3);
		fallback.total_metrics[index] = std::clamp(
			int(obs_data_get_int(settings,
					     (prefix + ".slot_" +
					      std::to_string(index + 1) + ".total_metric")
						     .c_str())),
			0, 2);
	}
	return fallback;
}
} // namespace

lol_dashboard_font_style lol_dashboard_font_style_from_settings(obs_data *settings,
								const char *role)
{
	auto *value = reinterpret_cast<obs_data_t *>(settings);
	const std::string prefix = std::string("lol_dashboard.typography.") + role;
	return {QString::fromUtf8(
			obs_data_get_string(value, (prefix + ".family").c_str())),
		float(obs_data_get_double(value, (prefix + ".optical_size").c_str())),
		float(obs_data_get_double(value, (prefix + ".weight").c_str())),
		float(obs_data_get_double(value, (prefix + ".width").c_str())),
		float(obs_data_get_double(value, (prefix + ".slant").c_str())),
		std::clamp(int(obs_data_get_int(value, (prefix + ".size").c_str())), 8,
			   100),
		obs_data_get_bool(value, (prefix + ".all_caps").c_str())};
}

lol_dashboard_regions lol_dashboard_regions_from_settings(obs_data *settings)
{
	auto *value = reinterpret_cast<obs_data_t *>(settings);
	lol_dashboard_regions result;
	result.top = section_from_settings(value, "top",
					   {true,
					    2,
					    {lol_dashboard_regions::widget::intensity,
					     lol_dashboard_regions::widget::intensity}});
	result.left =
		section_from_settings(value, "left",
				      {true,
				       3,
				       {lol_dashboard_regions::widget::mouse_activity,
					lol_dashboard_regions::widget::cumulative_totals,
					lol_dashboard_regions::widget::mouse_distance}});
	result.right = section_from_settings(value, "right",
					     {true,
					      2,
					      {lol_dashboard_regions::widget::live_keys,
					       lol_dashboard_regions::widget::top_keys}});
	return result;
}

lol_dashboard_trail_filter lol_dashboard_trail_filter_from_settings(obs_data *settings)
{
	auto *value = reinterpret_cast<obs_data_t *>(settings);
	lol_dashboard_trail_filter result;
	result.middle_clicks =
		obs_data_get_bool(value, "lol_dashboard.trail.middle_clicks");
	result.key_markers = obs_data_get_bool(value, "lol_dashboard.trail.key_markers");
	result.advanced = obs_data_get_bool(value, "lol_dashboard.trail.advanced");
	result.whitelist = obs_data_get_bool(value, "lol_dashboard.trail.whitelist");
	for (const auto &key :
	     QString::fromUtf8(obs_data_get_string(value, "lol_dashboard.trail.keys"))
		     .split(',', Qt::SkipEmptyParts))
		result.keys.push_back(key.trimmed());
	return result;
}
} // namespace sources
