#include "sources/game_report/collection/lol_v2_metrics.hpp"

#include <cassert>

int main()
{
	using namespace sources::lol_game_report;
	v2_metrics metrics;
	metrics.record_action(0.0, gameplay_input::left_click);
	metrics.record_action(1.0, gameplay_input::right_click);
	metrics.record_action(2.0, gameplay_input::bound_key);
	metrics.record_action(2.0, gameplay_input::middle_click);
	metrics.record_motion(0.0, {0, 0}, true);
	metrics.record_motion(1.0, {3, 4}, true);
	metrics.record_motion(2.0, {6, 8}, true);
	metrics.record_motion(3.0, {100, 100}, false);
	metrics.evaluate_through(4);
	const auto &series = metrics.intensity();
	assert(series.size() == 5);
	assert(series[2].apm == 60.0 && series[2].mouse_velocity == 10.0 / 3.0);
	assert(series[3].apm == 40.0 && series[3].mouse_velocity == 10.0 / 3.0);
	assert(series[4].apm == 20.0 && series[4].mouse_velocity == 5.0 / 3.0);
	const metric_summary summary = metrics.summary();
	assert(summary.left_clicks == 1 && summary.right_clicks == 1 && summary.gameplay_key_actions == 1);
	assert(summary.peak_apm == 60.0 && summary.median_apm == 40.0);
	metrics.reset(120);
	metrics.record_action(121.0, gameplay_input::left_click);
	metrics.evaluate_through(122);
	assert(metrics.intensity().size() == 3);
	assert(metrics.intensity().first().second == 120 && metrics.intensity().last().second == 122);
	return 0;
}
