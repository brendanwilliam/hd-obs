bool auto_detect_clicked(obs_properties_t *, obs_property_t *, void *data)
{
	static_cast<dashboard_source *>(data)->auto_detect();
	return true;
}
bool reload_clicked(obs_properties_t *, obs_property_t *, void *data)
{
	static_cast<dashboard_source *>(data)->reload();
	return true;
}
bool reset_clicked(obs_properties_t *, obs_property_t *, void *data)
{
	static_cast<dashboard_source *>(data)->reset_statistics();
	return true;
}
bool auto_link_captures_clicked(obs_properties_t *, obs_property_t *, void *data)
{
	static_cast<dashboard_source *>(data)->auto_link_captures();
	return true;
}
bool advanced_positioning_changed(obs_properties_t *props, obs_property_t *, obs_data_t *settings)
{
	const bool visible = obs_data_get_bool(settings, "lol_dashboard.advanced_positioning");
	for (const char *name :
	     {"lol_dashboard.frame_left", "lol_dashboard.frame_top", "lol_dashboard.screen_resolution"})
		obs_property_set_visible(obs_properties_get(props, name), visible);
	return true;
}
bool auto_switch_changed(obs_properties_t *props, obs_property_t *, obs_data_t *settings)
{
	const bool visible = obs_data_get_bool(settings, "lol_dashboard.auto_switch_captures");
	for (const char *name : {league_capture_switcher::game_source_key, league_capture_switcher::client_source_key,
				 "lol_dashboard.auto_link_captures"})
		obs_property_set_visible(obs_properties_get(props, name), visible);
	return true;
}
bool hud_section_changed(obs_properties_t *props, obs_property_t *property, obs_data_t *settings)
{
	std::string key = obs_property_name(property);
	const auto position = key.rfind('.');
	if (position == std::string::npos)
		return true;
	const std::string prefix = key.substr(0, position);
	const bool enabled = obs_data_get_bool(settings, (prefix + ".enabled").c_str());
	const int count = std::clamp(int(obs_data_get_int(settings, (prefix + ".count").c_str())), 1, 4);
	if (auto *count_property = obs_properties_get(props, (prefix + ".count").c_str()))
		obs_property_set_visible(count_property, enabled);
	for (int index = 0; index < 4; ++index)
		if (auto *slot = obs_properties_get(props, (prefix + ".slot_" + std::to_string(index + 1)).c_str())) {
			obs_property_set_visible(slot, enabled && index < count);
			const auto widget = static_cast<lol_dashboard_regions::widget>(std::clamp(
				int(obs_data_get_int(settings, (prefix + ".slot_" + std::to_string(index + 1)).c_str())),
				0, 6));
			if (auto *metric = obs_properties_get(
				    props,
				    (prefix + ".slot_" + std::to_string(index + 1) + ".intensity_metric").c_str()))
				obs_property_set_visible(metric,
							 enabled && index < count &&
								 widget == lol_dashboard_regions::widget::intensity);
			if (auto *metric = obs_properties_get(
				    props, (prefix + ".slot_" + std::to_string(index + 1) + ".total_metric").c_str()))
				obs_property_set_visible(
					metric, enabled && index < count &&
							widget == lol_dashboard_regions::widget::cumulative_totals);
		}
	return true;
}
bool analysis_changed(obs_properties_t *props, obs_property_t *, obs_data_t *settings)
{
	if (auto *hud = obs_properties_get(props, "lol_dashboard.hud_settings"))
		obs_property_set_visible(hud, obs_data_get_bool(settings, "lol_dashboard.report.analysis_enabled"));
	return true;
}
bool camera_changed(obs_properties_t *props, obs_property_t *, obs_data_t *settings)
{
	const bool visible = obs_data_get_bool(settings, "lol_dashboard.show_camera");
	for (const char *name : {"lol_dashboard.camera_source", "lol_dashboard.camera_next_to_minimap",
				 "lol_dashboard.camera_width_percent", "lol_dashboard.camera_height_percent",
				 "lol_dashboard.camera_scale_percent", "lol_dashboard.camera_translate_x_percent",
				 "lol_dashboard.camera_translate_y_percent", "lol_dashboard.camera_background_color"})
		obs_property_set_visible(obs_properties_get(props, name), visible);
	return true;
}
bool minimap_cover_changed(obs_properties_t *props, obs_property_t *, obs_data_t *settings)
{
	const bool visible = obs_data_get_bool(settings, "lol_dashboard.show_minimap_cover");
	const bool custom_image = visible && obs_data_get_bool(settings, "lol_dashboard.use_custom_minimap_cover");
	for (const char *name : {"lol_dashboard.use_custom_minimap_cover"})
		obs_property_set_visible(obs_properties_get(props, name), visible);
	for (const char *name :
	     {"lol_dashboard.minimap_cover_path", "lol_dashboard.minimap_cover_alpha_padding_percent",
	      "lol_dashboard.minimap_cover_width_percent", "lol_dashboard.minimap_cover_height_percent",
	      "lol_dashboard.minimap_cover_scale_percent", "lol_dashboard.minimap_cover_translate_x_percent",
	      "lol_dashboard.minimap_cover_translate_y_percent"})
		obs_property_set_visible(obs_properties_get(props, name), custom_image);
	return true;
}

struct camera_option_context {
	obs_source_t *dashboard;
	obs_property_t *property;
};
bool add_camera_source(void *data, obs_source_t *source)
{
	auto *context = static_cast<camera_option_context *>(data);
	if (source != context->dashboard && obs_source_get_type(source) == OBS_SOURCE_TYPE_INPUT &&
	    (obs_source_get_output_flags(source) & OBS_SOURCE_VIDEO))
		obs_property_list_add_string(context->property, obs_source_get_name(source),
					     obs_source_get_uuid(source));
	return true;
}

QString dashboard_source::screen_resolution_text() const
{
	return QString(obs_module_text("LoLPerformanceDashboard.ScreenResolution"))
		.arg(layout_ ? QString("%1x%2").arg(layout_->game.width).arg(layout_->game.height)
			     : obs_module_text("LoLPerformanceDashboard.Unavailable"));
}

obs_properties_t *properties(void *data)
{
	auto *properties = obs_properties_create();
	obs_properties_add_path(properties, path_key, obs_module_text("LeagueSafeArea.GameCfg"), OBS_PATH_FILE,
				"game.cfg", nullptr);
	obs_properties_add_button2(properties, "lol_dashboard.auto_detect",
				   obs_module_text("LeagueSafeArea.AutoDetect"), auto_detect_clicked, data);
	obs_properties_add_button2(properties, "lol_dashboard.reload", obs_module_text("LeagueSafeArea.Reload"),
				   reload_clicked, data);
	auto *cover = obs_properties_add_group(properties, "lol_dashboard.minimap_cover",
					       obs_module_text("LoLPerformanceDashboard.MinimapCover"),
					       OBS_GROUP_NORMAL, obs_properties_create());
	auto *cover_properties = obs_property_group_content(cover);
	auto *show_cover = obs_properties_add_bool(cover_properties, "lol_dashboard.show_minimap_cover",
						   obs_module_text("LoLPerformanceDashboard.ShowMinimapCover"));
	obs_property_set_modified_callback(show_cover, minimap_cover_changed);
	auto *use_custom_cover =
		obs_properties_add_bool(cover_properties, "lol_dashboard.use_custom_minimap_cover",
					obs_module_text("LoLPerformanceDashboard.UseCustomMinimapCover"));
	obs_property_set_modified_callback(use_custom_cover, minimap_cover_changed);
	obs_properties_add_path(cover_properties, "lol_dashboard.minimap_cover_path",
				obs_module_text("LoLPerformanceDashboard.MinimapCoverImage"), OBS_PATH_FILE,
				"Image files (*.png *.jpg *.jpeg *.webp *.bmp)", nullptr);
	obs_properties_add_int_slider(cover_properties, "lol_dashboard.minimap_cover_alpha_padding_percent",
				      obs_module_text("LoLPerformanceDashboard.MinimapCoverAlphaPadding"), 0, 50, 1);
	obs_properties_add_int_slider(cover_properties, "lol_dashboard.minimap_cover_width_percent",
				      obs_module_text("LoLPerformanceDashboard.WidthPercent"), 1, 200, 1);
	obs_properties_add_int_slider(cover_properties, "lol_dashboard.minimap_cover_height_percent",
				      obs_module_text("LoLPerformanceDashboard.HeightPercent"), 1, 200, 1);
	obs_properties_add_int_slider(cover_properties, "lol_dashboard.minimap_cover_scale_percent",
				      obs_module_text("LoLPerformanceDashboard.ScalePercent"), 1, 400, 1);
	obs_properties_add_int_slider(cover_properties, "lol_dashboard.minimap_cover_translate_x_percent",
				      obs_module_text("LoLPerformanceDashboard.TranslateXPercent"), -200, 200, 1);
	obs_properties_add_int_slider(cover_properties, "lol_dashboard.minimap_cover_translate_y_percent",
				      obs_module_text("LoLPerformanceDashboard.TranslateYPercent"), -200, 200, 1);

	auto *camera = obs_properties_add_group(properties, "lol_dashboard.camera",
						obs_module_text("LoLPerformanceDashboard.Camera"), OBS_GROUP_NORMAL,
						obs_properties_create());
	auto *camera_properties = obs_property_group_content(camera);
	auto *show_camera = obs_properties_add_bool(camera_properties, "lol_dashboard.show_camera",
						    obs_module_text("LoLPerformanceDashboard.ShowCamera"));
	obs_property_set_modified_callback(show_camera, camera_changed);
	auto *camera_source = obs_properties_add_list(camera_properties, "lol_dashboard.camera_source",
						      obs_module_text("LoLPerformanceDashboard.CameraSource"),
						      OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(camera_source, obs_module_text("LoLPerformanceDashboard.NoCamera"), "");
	if (data) {
		camera_option_context context{static_cast<dashboard_source *>(data)->source(), camera_source};
		obs_enum_sources(add_camera_source, &context);
	}
	auto *camera_location = obs_properties_add_list(camera_properties, "lol_dashboard.camera_next_to_minimap",
							obs_module_text("LoLPerformanceDashboard.CameraLocation"),
							OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_BOOL);
	obs_property_list_add_bool(camera_location, obs_module_text("LoLPerformanceDashboard.CameraNextToMinimap"),
				   true);
	obs_property_list_add_bool(camera_location, obs_module_text("LoLPerformanceDashboard.CameraOppositeMinimap"),
				   false);
	obs_properties_add_int_slider(camera_properties, "lol_dashboard.camera_width_percent",
				      obs_module_text("LoLPerformanceDashboard.WidthPercent"), 0, 100, 1);
	obs_properties_add_int_slider(camera_properties, "lol_dashboard.camera_height_percent",
				      obs_module_text("LoLPerformanceDashboard.HeightPercent"), 0, 200, 1);
	obs_properties_add_int_slider(camera_properties, "lol_dashboard.camera_scale_percent",
				      obs_module_text("LoLPerformanceDashboard.ScalePercent"), 1, 400, 1);
	obs_properties_add_int_slider(camera_properties, "lol_dashboard.camera_translate_x_percent",
				      obs_module_text("LoLPerformanceDashboard.TranslateXPercent"), -200, 200, 1);
	obs_properties_add_int_slider(camera_properties, "lol_dashboard.camera_translate_y_percent",
				      obs_module_text("LoLPerformanceDashboard.TranslateYPercent"), -200, 200, 1);
	obs_properties_add_color_alpha(camera_properties, "lol_dashboard.camera_background_color",
				       obs_module_text("LoLPerformanceDashboard.CameraBackgroundColor"));
	auto *capture_switching = obs_properties_add_group(properties, "lol_dashboard.capture_switching",
							   obs_module_text("LoLPerformanceDashboard.CaptureSwitching"),
							   OBS_GROUP_NORMAL, obs_properties_create());
	auto *capture_properties = obs_property_group_content(capture_switching);
	auto *auto_switch = obs_properties_add_bool(capture_properties, "lol_dashboard.auto_switch_captures",
						    obs_module_text("LoLPerformanceDashboard.AutoSwitchCaptures"));
	obs_property_set_modified_callback(auto_switch, auto_switch_changed);
	obs_properties_add_button2(capture_properties, "lol_dashboard.auto_link_captures",
				   obs_module_text("LoLPerformanceDashboard.AutoLinkCaptures"),
				   auto_link_captures_clicked, data);
	auto *client_capture = obs_properties_add_list(capture_properties, league_capture_switcher::client_source_key,
						       obs_module_text("LoLPerformanceDashboard.ClientCapture"),
						       OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
	auto *game_capture = obs_properties_add_list(capture_properties, league_capture_switcher::game_source_key,
						     obs_module_text("LoLPerformanceDashboard.GameCapture"),
						     OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
	for (obs_property_t *property : {game_capture, client_capture})
		obs_property_list_add_string(property, obs_module_text("LoLPerformanceDashboard.NoCapture"), "");
	for (const auto &[name, uuid] : league_capture_switcher::capture_sources()) {
		obs_property_list_add_string(game_capture, name.c_str(), uuid.c_str());
		obs_property_list_add_string(client_capture, name.c_str(), uuid.c_str());
	}
	auto *hands_diff = obs_properties_add_group(properties, "lol_dashboard.hands_diff",
						    obs_module_text("LoLGameReport"), OBS_GROUP_NORMAL,
						    obs_properties_create());
	auto *hands_diff_properties = obs_property_group_content(hands_diff);
	auto *frame = obs_properties_add_group(hands_diff_properties, "lol_dashboard.frame",
					       obs_module_text("LoLPerformanceDashboard.GameFrame"), OBS_GROUP_NORMAL,
					       obs_properties_create());
	auto *frame_properties = obs_property_group_content(frame);
	auto *advanced = obs_properties_add_bool(frame_properties, "lol_dashboard.advanced_positioning",
						 obs_module_text("LoLPerformanceDashboard.UltrawideMonitor"));
	obs_property_set_modified_callback(advanced, advanced_positioning_changed);
	obs_properties_add_int(frame_properties, "lol_dashboard.frame_left",
			       obs_module_text("LoLPerformanceDashboard.Left"), -32768, 32767, 1);
	obs_properties_add_int(frame_properties, "lol_dashboard.frame_top",
			       obs_module_text("LoLPerformanceDashboard.Top"), -32768, 32767, 1);
	if (data) {
		const QByteArray resolution = static_cast<dashboard_source *>(data)->screen_resolution_text().toUtf8();
		obs_properties_add_text(frame_properties, "lol_dashboard.screen_resolution", resolution.constData(),
					OBS_TEXT_INFO);
	}
	obs_properties_add_bool(hands_diff_properties, "lol_dashboard.report.analysis_enabled",
				obs_module_text("LoLGameReport.AnalysisEnabled"));
	obs_properties_add_button2(hands_diff_properties, "lol_dashboard.reset",
				   obs_module_text("LoLPerformanceDashboard.ResetStatistics"), reset_clicked, data);
	obs_properties_add_bool(hands_diff_properties, "lol_dashboard.reset_at_game_start",
				obs_module_text("LoLPerformanceDashboard.ResetAtGameStart"));
	obs_properties_add_bool(hands_diff_properties, "lol_dashboard.always_visible",
				obs_module_text("LoLPerformanceDashboard.DebugMode"));
	auto *hud = obs_properties_add_group(properties, "lol_dashboard.hud_settings",
					     obs_module_text("LoLPerformanceDashboard.InputHud"), OBS_GROUP_NORMAL,
					     obs_properties_create());
	auto *hud_properties = obs_property_group_content(hud);
	for (const auto &[name, title, horizontal] :
	     std::initializer_list<std::tuple<const char *, const char *, bool>>{
		     {"top", "LoLPerformanceDashboard.TopHud", true},
		     {"left", "LoLPerformanceDashboard.LeftHud", false},
		     {"right", "LoLPerformanceDashboard.RightHud", false}}) {
		const std::string prefix = std::string("lol_dashboard.hud.") + name;
		auto *section = obs_properties_add_group(hud_properties, prefix.c_str(), obs_module_text(title),
							 OBS_GROUP_NORMAL, obs_properties_create());
		auto *section_properties = obs_property_group_content(section);
		auto *enabled = obs_properties_add_bool(section_properties, (prefix + ".enabled").c_str(),
							obs_module_text("LoLPerformanceDashboard.EnableHud"));
		auto *count =
			obs_properties_add_int_slider(section_properties, (prefix + ".count").c_str(),
						      obs_module_text(horizontal ? "LoLPerformanceDashboard.Columns"
										 : "LoLPerformanceDashboard.Rows"),
						      1, 4, 1);
		obs_property_set_modified_callback(enabled, hud_section_changed);
		obs_property_set_modified_callback(count, hud_section_changed);
		for (int index = 0; index < 4; ++index) {
			const QByteArray widget_label =
				QString::fromUtf8(obs_module_text("LoLPerformanceDashboard.WidgetN"))
					.arg(index + 1)
					.toUtf8();
			const QByteArray metric_label =
				QString::fromUtf8(obs_module_text("LoLPerformanceDashboard.MetricN"))
					.arg(index + 1)
					.toUtf8();
			auto *slot = obs_properties_add_list(section_properties,
							     (prefix + ".slot_" + std::to_string(index + 1)).c_str(),
							     widget_label.constData(), OBS_COMBO_TYPE_LIST,
							     OBS_COMBO_FORMAT_INT);
			obs_property_list_add_int(slot, obs_module_text("LoLPerformanceDashboard.Widget.None"), 0);
			obs_property_set_modified_callback(slot, hud_section_changed);
			if (horizontal) {
				obs_property_list_add_int(
					slot, obs_module_text("LoLPerformanceDashboard.Widget.Intensity"), 1);
				obs_property_list_add_int(slot,
							  obs_module_text("LoLPerformanceDashboard.Widget.Totals"), 3);
				obs_property_list_add_int(
					slot, obs_module_text("LoLPerformanceDashboard.Widget.Distance"), 4);
				obs_property_list_add_int(
					slot, obs_module_text("LoLPerformanceDashboard.Widget.LiveKeys"), 5);
			} else {
				obs_property_list_add_int(
					slot, obs_module_text("LoLPerformanceDashboard.Widget.MouseActivity"), 2);
				obs_property_list_add_int(slot,
							  obs_module_text("LoLPerformanceDashboard.Widget.Totals"), 3);
				obs_property_list_add_int(
					slot, obs_module_text("LoLPerformanceDashboard.Widget.LiveKeys"), 5);
				obs_property_list_add_int(slot,
							  obs_module_text("LoLPerformanceDashboard.Widget.TopKeys"), 6);
			}
			auto *intensity_metric = obs_properties_add_list(
				section_properties,
				(prefix + ".slot_" + std::to_string(index + 1) + ".intensity_metric").c_str(),
				metric_label.constData(), OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
			obs_property_list_add_int(intensity_metric,
						  obs_module_text("LoLPerformanceDashboard.MouseVelocity"), 0);
			obs_property_list_add_int(intensity_metric, obs_module_text("LoLPerformanceDashboard.APM"), 1);
			obs_property_list_add_int(intensity_metric, obs_module_text("LoLPerformanceDashboard.CPM"), 2);
			obs_property_list_add_int(intensity_metric, obs_module_text("LoLPerformanceDashboard.KPM"), 3);
			auto *total_metric = obs_properties_add_list(
				section_properties,
				(prefix + ".slot_" + std::to_string(index + 1) + ".total_metric").c_str(),
				metric_label.constData(), OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
			obs_property_list_add_int(total_metric, obs_module_text("MouseActivity.Clicks"), 0);
			obs_property_list_add_int(total_metric, obs_module_text("LoLPerformanceDashboard.KeyPresses"),
						  1);
			obs_property_list_add_int(total_metric, obs_module_text("MouseActivity.Distance"), 2);
		}
	}
	if (data)
		static_cast<dashboard_source *>(data)->report_manager().add_properties(properties);
	auto *trail = obs_properties_add_group(hud_properties, "lol_dashboard.trail",
					       obs_module_text("LoLPerformanceDashboard.ClickTrail"), OBS_GROUP_NORMAL,
					       obs_properties_create());
	auto *trail_properties = obs_property_group_content(trail);
	obs_properties_add_bool(trail_properties, "lol_dashboard.trail.middle_clicks",
				obs_module_text("LoLPerformanceDashboard.TrailMiddle"));
	obs_properties_add_bool(trail_properties, "lol_dashboard.trail.key_markers",
				obs_module_text("LoLPerformanceDashboard.TrailKeys"));
	obs_properties_add_bool(trail_properties, "lol_dashboard.trail.advanced",
				obs_module_text("LoLPerformanceDashboard.TrailAdvanced"));
	obs_properties_add_bool(trail_properties, "lol_dashboard.trail.whitelist",
				obs_module_text("LoLPerformanceDashboard.TrailWhitelist"));
	obs_properties_add_text(trail_properties, "lol_dashboard.trail.keys",
				obs_module_text("LoLPerformanceDashboard.TrailKeyList"), OBS_TEXT_DEFAULT);
	if (auto *analysis = obs_properties_get(properties, "lol_dashboard.report.analysis_enabled"))
		obs_property_set_modified_callback(analysis, analysis_changed);

	if (data) {
		auto *settings = obs_source_get_settings(static_cast<dashboard_source *>(data)->source());
		camera_changed(camera_properties, nullptr, settings);
		minimap_cover_changed(cover_properties, nullptr, settings);
		auto_switch_changed(capture_properties, nullptr, settings);
		analysis_changed(properties, nullptr, settings);
		obs_data_release(settings);
	}
	if (data) {
		auto *settings = obs_source_get_settings(static_cast<dashboard_source *>(data)->source());
		advanced_positioning_changed(frame_properties, nullptr, settings);
		obs_data_release(settings);
	}
	return properties;
}
