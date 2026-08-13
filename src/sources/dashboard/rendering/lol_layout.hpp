#pragma once

#include "sources/hud_layout/lol_layout.hpp"

#include <array>

namespace sources {

enum class lol_dashboard_alignment { left, center, right };

struct lol_dashboard_rect {
	int x_{};
	int y_{};
	int width_{};
	int height_{};

	constexpr int x() const { return x_; }
	constexpr int y() const { return y_; }
	constexpr int width() const { return width_; }
	constexpr int height() const { return height_; }
	constexpr int left() const { return x_; }
	constexpr int top() const { return y_; }
	constexpr int right() const { return x_ + width_ - 1; }
	constexpr int bottom() const { return y_ + height_ - 1; }
	constexpr bool isEmpty() const { return width_ < 1 || height_ < 1; }
	void moveLeft(int value) { x_ = value; }
	void moveTop(int value) { y_ = value; }
	void translate(int horizontal, int vertical)
	{
		x_ += horizontal;
		y_ += vertical;
	}
	lol_dashboard_rect adjusted(int left_adjustment, int top_adjustment, int right_adjustment,
				    int bottom_adjustment) const
	{
		return {x_ + left_adjustment, y_ + top_adjustment, width_ + right_adjustment - left_adjustment,
			height_ + bottom_adjustment - top_adjustment};
	}
};

struct lol_dashboard_camera_layout {
	bool enabled{};
	bool next_to_minimap{};
	double aspect{1.0};
	int width_percent{100};
	int height_percent{100};
	int scale_percent{100};
	int translate_x_percent{};
	int translate_y_percent{};
};

struct lol_dashboard_image_layout {
	double aspect{1.0};
	int width_percent{100};
	int height_percent{100};
	int scale_percent{100};
	int translate_x_percent{};
	int translate_y_percent{};
	int alpha_padding_percent{};
	bool fit_within_mask{};
};

struct lol_dashboard_panels {
	lol_dashboard_rect header, heatmap, summary, left_top_keys, keys, camera_mask, camera, minimap_cover_mask,
		minimap_cover;
	bool camera_visible{};
};

// A v2 HUD section is split into equal slots by its caller.  Keeping this
// geometry independent of rendering makes every widget usable in every
// permitted section.
std::array<lol_dashboard_rect, 4> lol_dashboard_split_slots(const lol_dashboard_rect &bounds, int count,
							    bool horizontal, int gap = 10);
std::array<lol_dashboard_rect, 4> lol_dashboard_split_weighted_slots(const lol_dashboard_rect &bounds,
								     const std::array<int, 4> &weights, int count,
								     bool horizontal, int gap = 10);
std::array<lol_dashboard_rect, 4> lol_dashboard_stack_slots(const lol_dashboard_rect &bounds,
							    const std::array<int, 4> &heights, int count, int gap = 10);
// The activity map and no-camera Top Keys chart occupy dedicated regions.
// Other left-HUD widgets use the summary region without changing their slots.
std::array<lol_dashboard_rect, 4>
lol_dashboard_left_widget_slots(const lol_dashboard_rect &heatmap, const lol_dashboard_rect &summary,
				const lol_dashboard_rect &top_keys, const std::array<int, 4> &heights,
				const std::array<bool, 4> &mouse_activity, const std::array<bool, 4> &top_key_widgets,
				int count, int gap = 10);

lol_dashboard_rect lol_dashboard_aspect_fit(const lol_dashboard_rect &bounds, double aspect);
lol_dashboard_rect lol_dashboard_aspect_fit_aligned(const lol_dashboard_rect &bounds, double aspect,
						    lol_dashboard_alignment alignment);
lol_dashboard_rect lol_dashboard_aspect_fit_left(const lol_dashboard_rect &bounds, double aspect);

lol_dashboard_panels lol_dashboard_panel_rectangles(const league_safe_area::model &layout,
						    const lol_dashboard_camera_layout &camera,
						    const lol_dashboard_image_layout &minimap_cover,
						    int hud_padding = 20);

} // namespace sources
