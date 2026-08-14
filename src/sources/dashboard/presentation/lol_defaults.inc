void defaults(obs_data_t *settings)
{
	lol_report_manager::defaults(settings);
	obs_data_set_default_bool(settings, "lol_dashboard.advanced_positioning", false);
	obs_data_set_default_bool(settings, "lol_dashboard.always_visible", false);
	obs_data_set_default_bool(settings, "lol_dashboard.reset_at_game_start", true);
	obs_data_set_default_bool(settings, "lol_dashboard.auto_switch_captures", true);
	obs_data_set_default_string(settings, league_capture_switcher::game_source_key,
				    "");
	obs_data_set_default_string(settings, league_capture_switcher::client_source_key,
				    "");
	obs_data_set_default_bool(settings, "lol_dashboard.show_intensity", true);
	obs_data_set_default_bool(settings, "lol_dashboard.show_keys", true);
	obs_data_set_default_bool(settings, "lol_dashboard.show_mouse_activity", true);
	obs_data_set_default_bool(settings, "lol_dashboard.show_camera", true);
	obs_data_set_default_string(settings, "lol_dashboard.camera_source", "");
	obs_data_set_default_bool(settings, "lol_dashboard.camera_next_to_minimap",
				  false);
	obs_data_set_default_int(settings, "lol_dashboard.camera_width_percent", 100);
	obs_data_set_default_int(settings, "lol_dashboard.camera_height_percent", 100);
	obs_data_set_default_int(settings, "lol_dashboard.camera_scale_percent", 100);
	obs_data_set_default_int(settings, "lol_dashboard.camera_translate_x_percent", 0);
	obs_data_set_default_int(settings, "lol_dashboard.camera_translate_y_percent", 0);
	obs_data_set_default_int(settings, "lol_dashboard.camera_background_color",
				 0xff1a1a1a);
	obs_data_set_default_bool(settings, "lol_dashboard.show_minimap_cover", true);
	obs_data_set_default_bool(settings, "lol_dashboard.use_custom_minimap_cover",
				  false);
	obs_data_set_default_string(settings, "lol_dashboard.minimap_cover_path", "");
	obs_data_set_default_int(settings,
				 "lol_dashboard.minimap_cover_alpha_padding_percent", 0);
	obs_data_set_default_int(settings, "lol_dashboard.minimap_cover_width_percent",
				 100);
	obs_data_set_default_int(settings, "lol_dashboard.minimap_cover_height_percent",
				 100);
	obs_data_set_default_int(settings, "lol_dashboard.minimap_cover_scale_percent",
				 100);
	obs_data_set_default_int(settings,
				 "lol_dashboard.minimap_cover_translate_x_percent", 0);
	obs_data_set_default_int(settings,
				 "lol_dashboard.minimap_cover_translate_y_percent", 0);
	obs_data_set_default_int(settings, "activity.inactive_color", 0xff425e62);
	obs_data_set_default_int(settings, "activity.active_color", 0xff83c1dd);
	obs_data_set_default_int(settings, "activity.background_color", 0x00000000);
	obs_data_set_default_int(settings, "lol_dashboard.section_padding", 20);
	obs_data_set_default_int(settings, "lol_dashboard.element_padding", 20);
	obs_data_set_default_int(settings, "lol_dashboard.element_x_gap", 10);
	obs_data_set_default_int(settings, "lol_dashboard.element_y_gap", 10);
	obs_data_set_default_int(settings, "lol_dashboard.within_element_gap", 10);
	obs_data_set_default_int(settings, "lol_dashboard.label_spacing", 10);
	obs_data_set_default_int(settings, "lol_dashboard.intensity_padding", 120);
	for (const auto &[section, enabled, count, values] :
	     std::initializer_list<std::tuple<const char *, bool, int, std::array<int, 4>>>{
		     {"top", true, 2, {1, 1, 0, 0}},
		     {"left", true, 3, {2, 3, 4, 0}},
		     {"right", true, 2, {5, 6, 0, 0}}}) {
		const std::string prefix = std::string("lol_dashboard.hud.") + section;
		obs_data_set_default_bool(settings, (prefix + ".enabled").c_str(),
					  enabled);
		obs_data_set_default_int(settings, (prefix + ".count").c_str(), count);
		for (int index = 0; index < 4; ++index)
			obs_data_set_default_int(
				settings,
				(prefix + ".slot_" + std::to_string(index + 1)).c_str(),
				values[index]);
		for (int index = 0; index < 4; ++index) {
			obs_data_set_default_int(settings,
						 (prefix + ".slot_" +
						  std::to_string(index + 1) +
						  ".intensity_metric")
							 .c_str(),
						 0);
			obs_data_set_default_int(settings,
						 (prefix + ".slot_" +
						  std::to_string(index + 1) +
						  ".total_metric")
							 .c_str(),
						 0);
		}
	}
	obs_data_set_default_bool(settings, "lol_dashboard.trail.middle_clicks", false);
	obs_data_set_default_bool(settings, "lol_dashboard.trail.key_markers", false);
	obs_data_set_default_bool(settings, "lol_dashboard.trail.advanced", false);
	obs_data_set_default_bool(settings, "lol_dashboard.trail.whitelist", false);
	obs_data_set_default_string(settings, "lol_dashboard.trail.keys", "");
	for (const auto &[role, family, size] :
	     std::initializer_list<std::tuple<const char *, const char *, int>>{
		     {"numbers", "Inter", 30},
		     {"numbers_secondary", "Inter", 18},
		     {"number_labels", "Inter", 18},
		     {"button_labels", "Inter", 30}}) {
		const std::string prefix =
			std::string("lol_dashboard.typography.") + role;
		obs_data_set_default_string(settings, (prefix + ".family").c_str(),
					    family);
		obs_data_set_default_double(settings, (prefix + ".optical_size").c_str(),
					    22.0);
		obs_data_set_default_double(settings, (prefix + ".weight").c_str(),
					    700.0);
		obs_data_set_default_double(settings, (prefix + ".width").c_str(), 100.0);
		obs_data_set_default_double(settings, (prefix + ".slant").c_str(), 0.0);
		obs_data_set_default_int(settings, (prefix + ".size").c_str(), size);
		obs_data_set_default_bool(settings, (prefix + ".all_caps").c_str(),
					  false);
	}
}
