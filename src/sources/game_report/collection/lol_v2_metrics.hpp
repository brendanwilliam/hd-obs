#pragma once

#include <QPointF>
#include <QVector>

namespace sources::lol_game_report {

enum class gameplay_input { left_click, right_click, middle_click, bound_key };

struct intensity_sample {
	int second{};
	double apm{};
	double mouse_velocity{};
};

struct metric_summary {
	int left_clicks{};
	int right_clicks{};
	int gameplay_key_actions{};
	double peak_apm{};
	double median_apm{};
	double peak_mouse_velocity{};
	double median_mouse_velocity{};
};

class v2_metrics {
public:
	void reset();
	void record_action(double game_seconds, gameplay_input input);
	void record_motion(double game_seconds, QPointF point, bool in_game_frame);
	void evaluate_through(int game_second);
	const QVector<intensity_sample> &intensity() const;
	metric_summary summary() const;

private:
	struct action {
		double seconds{};
		gameplay_input input{};
	};
	struct motion_segment {
		double ended_at{};
		double distance{};
	};
	QVector<action> actions_;
	QVector<motion_segment> motion_;
	QVector<intensity_sample> intensity_;
	QPointF previous_point_;
	double previous_motion_seconds_{};
	bool has_previous_point_{};
	int left_clicks_{}, right_clicks_{}, gameplay_key_actions_{}, last_second_{-1};
};

} // namespace sources::lol_game_report
