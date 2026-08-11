#include "sources/game_report/collection/lol_v2_metrics.hpp"

#include <algorithm>
#include <cmath>

namespace sources::lol_game_report {
void v2_metrics::reset()
{
	actions_.clear();
	motion_.clear();
	intensity_.clear();
	has_previous_point_ = false;
	left_clicks_ = right_clicks_ = gameplay_key_actions_ = 0;
	last_second_ = -1;
}

void v2_metrics::record_action(double game_seconds, gameplay_input input)
{
	if (game_seconds < 0)
		return;
	if (input == gameplay_input::left_click)
		++left_clicks_;
	else if (input == gameplay_input::right_click)
		++right_clicks_;
	else if (input == gameplay_input::bound_key)
		++gameplay_key_actions_;
	if (input != gameplay_input::middle_click)
		actions_.append({game_seconds, input});
}

void v2_metrics::record_motion(double game_seconds, QPointF point, bool in_game_frame)
{
	if (!in_game_frame || game_seconds < 0) {
		has_previous_point_ = false;
		return;
	}
	if (has_previous_point_ && game_seconds >= previous_motion_seconds_)
		motion_.append(
			{game_seconds, std::hypot(point.x() - previous_point_.x(), point.y() - previous_point_.y())});
	previous_point_ = point;
	previous_motion_seconds_ = game_seconds;
	has_previous_point_ = true;
}

void v2_metrics::evaluate_through(int game_second)
{
	for (int second = std::max(0, last_second_ + 1); second <= game_second; ++second) {
		int actions{};
		double distance{};
		for (const auto &action : actions_)
			if (action.seconds > second - 3 && action.seconds <= second)
				++actions;
		for (const auto &segment : motion_)
			if (segment.ended_at > second - 3 && segment.ended_at <= second)
				distance += segment.distance;
		intensity_.append({second, actions * 20.0, distance / 3.0});
		last_second_ = second;
	}
}

const QVector<intensity_sample> &v2_metrics::intensity() const
{
	return intensity_;
}

metric_summary v2_metrics::summary() const
{
	metric_summary result{left_clicks_, right_clicks_, gameplay_key_actions_};
	QVector<double> apm, velocity;
	for (const auto &sample : intensity_) {
		apm.append(sample.apm);
		velocity.append(sample.mouse_velocity);
		result.peak_apm = std::max(result.peak_apm, sample.apm);
		result.peak_mouse_velocity = std::max(result.peak_mouse_velocity, sample.mouse_velocity);
	}
	auto median = [](QVector<double> values) {
		if (values.isEmpty())
			return 0.0;
		std::sort(values.begin(), values.end());
		const int middle = values.size() / 2;
		return values.size() % 2 ? values[middle] : (values[middle - 1] + values[middle]) / 2.0;
	};
	result.median_apm = median(apm);
	result.median_mouse_velocity = median(velocity);
	return result;
}
} // namespace sources::lol_game_report
