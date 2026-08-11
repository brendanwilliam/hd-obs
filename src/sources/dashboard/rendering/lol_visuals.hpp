#pragma once

#include "input/input_data.hpp"
#include "sources/dashboard/rendering/lol_layout.hpp"

#include <QColor>
#include <QHash>
#include <QPointF>
#include <QRect>
#include <array>
#include <cstdint>
#include <deque>
#include <optional>
#include <unordered_map>
#include <vector>

class QPainter;

namespace sources {

struct lol_dashboard_theme {
	QColor inactive;
	QColor active;
	QColor background;
};

enum class lol_dashboard_alignment { left, center, right };

struct lol_dashboard_regions {
	enum class widget { none, intensity, mouse_activity, cumulative_totals, mouse_distance, live_keys, top_keys };
	struct section {
		bool enabled{};
		int count{};
		std::array<widget, 4> widgets{};
		std::array<int, 4> intensity_metrics{};
		std::array<int, 4> total_metrics{};
	};
	section top{true, 2, {widget::intensity, widget::intensity}};
	section left{true, 3, {widget::mouse_activity, widget::cumulative_totals, widget::mouse_distance}};
	section right{true, 2, {widget::live_keys, widget::top_keys}};
};

int lol_dashboard_widget_layout_weight(lol_dashboard_regions::widget widget, bool horizontal);

struct lol_dashboard_trail_filter {
	bool middle_clicks{};
	bool key_markers{};
	bool advanced{};
	bool whitelist{};
	std::vector<QString> keys;
};

struct lol_dashboard_font_style {
	QString family{"Science Gothic"};
	float optical_size{22.0F};
	float weight{700.0F};
	float width{100.0F};
	float slant{0.0F};
	int size{22};
	bool all_caps{false};
};

// Camera and minimap placement deliberately live in lol_layout and are not
// affected by this HUD style.
struct lol_dashboard_style {
	int section_padding{20};
	int element_padding{20};
	int element_x_gap{10};
	int element_y_gap{10};
	int within_element_gap{10};
	int label_spacing{10};
	int intensity_padding{120};
	lol_dashboard_font_style number_primary{"Inter", 22.0F, 700.0F, 100.0F, 0.0F, 30};
	lol_dashboard_font_style numbers_secondary{"Inter", 22.0F, 700.0F, 100.0F, 0.0F, 18};
	lol_dashboard_font_style number_labels{"Inter", 22.0F, 700.0F, 100.0F, 0.0F, 18};
	lol_dashboard_font_style button_labels{"Inter", 22.0F, 700.0F, 100.0F, 0.0F, 30};
};

void lol_dashboard_draw_shadowed_text(QPainter &painter, const QRect &bounds, Qt::Alignment alignment,
				      const QString &text);
QRect lol_dashboard_heatmap_content_bounds(const QRect &bounds, const QRect &game_frame,
					   const lol_dashboard_style &style);

class lol_dashboard_visuals {
public:
	void configure(const lol_dashboard_theme &theme, const lol_dashboard_regions &regions,
		       int rolling_window_seconds, const QRect &game_frame, const QRect &pointer_bounds,
		       const lol_dashboard_style &style, const lol_dashboard_trail_filter &trail_filter);
	void set_gameplay_actions(const QHash<QString, QString> &actions);
	void consume(const std::vector<input_data::trace_event> &events,
		     const input_data::button_map<uint16_t> &keyboard, const input_data::button_map<uint16_t> &mouse);
	void clear_live_keys();
	void reset();
	void draw(QPainter &painter, const std::array<QRect, 4> &top, const std::array<QRect, 4> &left,
		  const std::array<QRect, 4> &right) const;

private:
	struct trail_event {
		QPointF point;
		uint64_t time_ns{};
		uint16_t button{};
		QString label;
	};
	struct motion_sample {
		QPointF point;
		uint64_t time_ns{};
	};
	struct pointer_indicator {
		uint16_t code{};
		QString label;
		uint64_t fade_started{};
		uint64_t fade_until{};
	};
	struct active_key {
		uint16_t code;
		QString label;
		uint64_t fade_started{};
		uint64_t fade_until{};
		uint64_t count{};
	};
	void advance(uint64_t now);
	void on_event(const input_data::trace_event &event);
	void activate_pointer_indicator(uint16_t code, const QString &label);
	void release_pointer_indicator(uint16_t code, uint64_t now);
	void draw_pointer(QPainter &painter, const QRect &bounds) const;
	void draw_cumulative_totals(QPainter &painter, const QRect &bounds, lol_dashboard_alignment alignment,
				    int metric) const;
	void draw_mouse_distance(QPainter &painter, const QRect &bounds, lol_dashboard_alignment alignment) const;
	void draw_live_keys(QPainter &painter, const QRect &bounds, lol_dashboard_alignment alignment) const;
	void draw_top_keys(QPainter &painter, const QRect &bounds, lol_dashboard_alignment alignment) const;
	void draw_intensity(QPainter &painter, const QRect &bounds, int metric) const;
	void draw_widget(QPainter &painter, lol_dashboard_regions::widget widget, const QRect &bounds,
			 int intensity_metric, int total_metric, lol_dashboard_alignment alignment) const;
	bool accepts_key(const QString &label) const;
	QString distance_label() const;

	lol_dashboard_theme theme_{{98, 94, 66}, {221, 193, 131}, {0, 0, 0, 0}};
	lol_dashboard_regions regions_;
	lol_dashboard_trail_filter trail_filter_;
	lol_dashboard_style style_;
	QRect game_frame_{0, 0, 1920, 1080};
	QRect pointer_bounds_;
	std::optional<QPointF> pointer_;
	std::deque<trail_event> trail_;
	std::deque<motion_sample> motion_trail_;
	std::vector<pointer_indicator> pointer_indicators_;
	input_data::button_map<uint16_t> mouse_;
	QHash<QString, QString> gameplay_actions_;
	std::optional<QPoint> last_distance_;
	std::optional<input_data::trace_event> last_motion_;
	std::unordered_map<uint16_t, bool> held_;
	std::unordered_map<uint16_t, uint64_t> press_counts_;
	std::vector<active_key> active_keys_;
	std::deque<std::array<double, 2>> samples_;
	std::vector<std::array<double, 2>> session_samples_;
	std::array<double, 2> current_{};
	uint64_t bucket_start_{}, total_clicks_{}, total_key_presses_{};
	double distance_{};
	int window_{60};
};

} // namespace sources
