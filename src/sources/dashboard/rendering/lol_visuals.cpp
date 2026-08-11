#include "sources/dashboard/rendering/lol_visuals.hpp"
#include "sources/dashboard/rendering/lol_key_labels.hpp"
#include "input/keycodes.h"

#include <QFont>
#include <QFontDatabase>
#include <QFontMetrics>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>
#include <QStringList>
#include <algorithm>
#include <cmath>
#include <obs-module.h>
#include <util/platform.h>

namespace sources {
namespace {
constexpr uint64_t second_ns = 1000000000ULL;
constexpr uint64_t live_key_fade_ns = 1500000000ULL;
constexpr uint64_t mouse_trail_segment_ns = 20000000ULL;
constexpr uint64_t mouse_trail_duration_ns = 1500000000ULL;
constexpr uint64_t pointer_indicator_fade_ns = 500000000ULL;

void ensure_dashboard_fonts_registered()
{
	static const bool registered = [] {
		for (const char *resource : {"fonts/ScienceGothic.ttf", "fonts/Inter.ttf"}) {
			char *path = obs_module_file(resource);
			if (path) {
				QFontDatabase::addApplicationFont(QString::fromUtf8(path));
				bfree(path);
			}
		}
		return true;
	}();
	Q_UNUSED(registered);
}

QFont dashboard_font(const lol_dashboard_font_style &style, QFont::Weight weight = QFont::Normal)
{
	QFont font(style.family, style.size, weight);
	font.setVariableAxis(QFont::Tag("opsz"), style.optical_size);
	font.setVariableAxis(QFont::Tag("wght"), style.weight);
	if (style.family == "Science Gothic") {
		font.setVariableAxis(QFont::Tag("wdth"), style.width);
		font.setVariableAxis(QFont::Tag("slnt"), style.slant);
	}
	return font;
}

QString dashboard_text(const QString &text, const lol_dashboard_font_style &style)
{
	return style.all_caps ? text.toUpper() : text;
}

Qt::Alignment horizontal_alignment(lol_dashboard_alignment alignment)
{
	switch (alignment) {
	case lol_dashboard_alignment::left:
		return Qt::AlignLeft;
	case lol_dashboard_alignment::center:
		return Qt::AlignHCenter;
	case lol_dashboard_alignment::right:
		return Qt::AlignRight;
	}
	return Qt::AlignLeft;
}
} // namespace

void lol_dashboard_visuals::set_gameplay_actions(const QHash<QString, QString> &actions)
{
	gameplay_actions_ = actions;
}

void lol_dashboard_visuals::configure(const lol_dashboard_theme &theme, const lol_dashboard_regions &regions,
				      int rolling_window_seconds, const QRect &game_frame, const QRect &pointer_bounds,
				      const lol_dashboard_style &style, const lol_dashboard_trail_filter &trail_filter)
{
	theme_ = theme;
	regions_ = regions;
	style_ = style;
	trail_filter_ = trail_filter;
	window_ = std::clamp(rolling_window_seconds, 1, 60);
	game_frame_ = game_frame;
	pointer_bounds_ = lol_dashboard_heatmap_content_bounds(pointer_bounds, game_frame_, style_);
}

bool lol_dashboard_visuals::accepts_key(const QString &label) const
{
	if (!trail_filter_.advanced || trail_filter_.keys.empty())
		return true;
	const bool listed = std::any_of(trail_filter_.keys.begin(), trail_filter_.keys.end(), [&](const QString &key) {
		return key.compare(label, Qt::CaseInsensitive) == 0;
	});
	return trail_filter_.whitelist ? listed : !listed;
}

QRect lol_dashboard_heatmap_content_bounds(const QRect &bounds, const QRect &game_frame,
					   const lol_dashboard_style &style)
{
	Q_UNUSED(style);
	if (bounds.isEmpty() || game_frame.width() < 1 || game_frame.height() < 1)
		return {};
	const auto fitted = lol_dashboard_aspect_fit({bounds.x(), bounds.y(), bounds.width(), bounds.height()},
						     double(game_frame.width()) / game_frame.height());
	return {fitted.x(), fitted.y(), fitted.width(), fitted.height()};
}

int lol_dashboard_widget_layout_weight(lol_dashboard_regions::widget widget, bool horizontal)
{
	switch (widget) {
	case lol_dashboard_regions::widget::intensity:
		return horizontal ? 3 : 2;
	case lol_dashboard_regions::widget::live_keys:
		return horizontal ? 3 : 2;
	case lol_dashboard_regions::widget::top_keys:
		return horizontal ? 3 : 3;
	case lol_dashboard_regions::widget::mouse_activity:
		return horizontal ? 3 : 4;
	case lol_dashboard_regions::widget::cumulative_totals:
	case lol_dashboard_regions::widget::mouse_distance:
		return horizontal ? 2 : 1;
	case lol_dashboard_regions::widget::none:
		return 1;
	}
	return 1;
}

void lol_dashboard_visuals::consume(const std::vector<input_data::trace_event> &events,
				    const input_data::button_map<uint16_t> &keyboard,
				    const input_data::button_map<uint16_t> &mouse)
{
	mouse_ = mouse;
	for (const auto &event : events)
		on_event(event);
	const uint64_t now = os_gettime_ns();
	advance(now);
	for (auto it = active_keys_.begin(); it != active_keys_.end();) {
		const auto found = keyboard.find(it->code);
		if (found != keyboard.end() && found->second) {
			held_[it->code] = true;
			it->fade_started = it->fade_until = 0;
			++it;
		} else {
			held_[it->code] = false;
			if (!it->fade_until) {
				it->fade_started = now;
				it->fade_until = now + live_key_fade_ns;
			}
			if (it->fade_until <= now)
				it = active_keys_.erase(it);
			else
				++it;
		}
	}
	pointer_indicators_.erase(std::remove_if(pointer_indicators_.begin(), pointer_indicators_.end(),
						 [&](const auto &indicator) {
							 return indicator.fade_until && indicator.fade_until <= now;
						 }),
				  pointer_indicators_.end());
}

void lol_dashboard_visuals::clear_live_keys()
{
	held_.clear();
	active_keys_.clear();
	pointer_indicators_.clear();
}

void lol_dashboard_visuals::activate_pointer_indicator(uint16_t code, const QString &label)
{
	for (auto &indicator : pointer_indicators_)
		if (indicator.code == code) {
			indicator.label = label;
			indicator.fade_started = indicator.fade_until = 0;
			return;
		}
	pointer_indicators_.push_back({code, label, 0, 0});
}

void lol_dashboard_visuals::release_pointer_indicator(uint16_t code, uint64_t now)
{
	for (auto &indicator : pointer_indicators_)
		if (indicator.code == code) {
			indicator.fade_started = now;
			indicator.fade_until = now + pointer_indicator_fade_ns;
			return;
		}
}

void lol_dashboard_visuals::advance(uint64_t now)
{
	if (!bucket_start_)
		bucket_start_ = now;
	while (now - bucket_start_ >= second_ns) {
		samples_.push_back(current_);
		session_samples_.push_back(current_);
		if (samples_.size() > size_t(window_))
			samples_.pop_front();
		current_.fill(0.0);
		bucket_start_ += second_ns;
	}
}
void lol_dashboard_visuals::on_event(const input_data::trace_event &event)
{
	advance(event.time_ns);
	if (event.type == EVENT_KEY_PRESSED && !held_[event.code]) {
		held_[event.code] = true;
		active_keys_.erase(std::remove_if(active_keys_.begin(), active_keys_.end(),
						  [&](const auto &key) { return key.code == event.code; }),
				   active_keys_.end());
		active_keys_.push_back(
			{event.code, lol_dashboard_key_label(event.code), 0, 0, ++press_counts_[event.code]});
		++current_[1];
		++total_key_presses_;
		activate_pointer_indicator(event.code, lol_dashboard_key_label(event.code));
	} else if (event.type == EVENT_KEY_RELEASED) {
		held_[event.code] = false;
		for (auto &key : active_keys_)
			if (key.code == event.code) {
				key.fade_started = event.time_ns;
				key.fade_until = event.time_ns + live_key_fade_ns;
			}
		release_pointer_indicator(event.code, event.time_ns);
	} else if (event.type == EVENT_MOUSE_PRESSED) {
		++current_[1];
		++total_clicks_;
		activate_pointer_indicator(event.code, event.code == MOUSE_BUTTON1   ? "L"
						       : event.code == MOUSE_BUTTON2 ? "R"
										     : "M");
		const bool supported = event.code == MOUSE_BUTTON1 || event.code == MOUSE_BUTTON2 ||
				       (trail_filter_.middle_clicks && event.code == MOUSE_BUTTON3);
		if (supported && game_frame_.contains(event.x, event.y)) {
			const QPointF point(double(event.x - game_frame_.left()) / std::max(1, game_frame_.width()),
					    double(event.y - game_frame_.top()) / std::max(1, game_frame_.height()));
			trail_.push_back({point, event.time_ns, event.code, {}});
			if (trail_.size() > 20)
				trail_.pop_front();
		}
	} else if (event.type == EVENT_MOUSE_RELEASED) {
		release_pointer_indicator(event.code, event.time_ns);
	}
	if (event.type == EVENT_KEY_PRESSED && trail_filter_.key_markers && game_frame_.contains(event.x, event.y)) {
		const QString label = lol_dashboard_key_label(event.code);
		if (accepts_key(label)) {
			const QPointF point(double(event.x - game_frame_.left()) / std::max(1, game_frame_.width()),
					    double(event.y - game_frame_.top()) / std::max(1, game_frame_.height()));
			trail_.push_back({point, event.time_ns, 0, label});
			if (trail_.size() > 20)
				trail_.pop_front();
		}
	}
	if (event.type == EVENT_KEY_PRESSED && pointer_) {
		QStringList chord;
		const auto append_modifier = [&](uint16_t left, uint16_t right, const char *name) {
			if ((held_.count(left) && held_.at(left)) || (held_.count(right) && held_.at(right)))
				chord.append(name);
		};
		append_modifier(VC_SHIFT_L, VC_SHIFT_R, "Shift");
		append_modifier(VC_CONTROL_L, VC_CONTROL_R, "Ctrl");
		append_modifier(VC_ALT_L, VC_ALT_R, "Alt");
		append_modifier(VC_META_L, VC_META_R, "Cmd");
		std::sort(chord.begin(), chord.end());
		chord.append(lol_dashboard_key_label(event.code));
		const auto action = gameplay_actions_.constFind(chord.join('+'));
		if (action != gameplay_actions_.cend()) {
			trail_.push_back({*pointer_, event.time_ns, 0, lol_dashboard_key_label(event.code).toLower()});
			if (trail_.size() > 20)
				trail_.pop_front();
		}
	}
	if (event.type != EVENT_MOUSE_MOVED && event.type != EVENT_MOUSE_DRAGGED)
		return;
	if (last_motion_)
		current_[0] += std::hypot(event.x - last_motion_->x, event.y - last_motion_->y);
	if (!game_frame_.contains(QPoint(event.x, event.y))) {
		last_motion_ = event;
		last_distance_.reset();
		return;
	}
	const QPoint relative(event.x - game_frame_.left(), event.y - game_frame_.top());
	if (last_distance_)
		distance_ += std::hypot(relative.x() - last_distance_->x(), relative.y() - last_distance_->y());
	last_distance_ = relative;
	pointer_ = {double(relative.x()) / std::max(1, game_frame_.width()),
		    double(relative.y()) / std::max(1, game_frame_.height())};
	if (motion_trail_.empty() || event.time_ns - motion_trail_.back().time_ns >= mouse_trail_segment_ns)
		motion_trail_.push_back({*pointer_, event.time_ns});
	while (!motion_trail_.empty() && event.time_ns - motion_trail_.front().time_ns > mouse_trail_duration_ns)
		motion_trail_.pop_front();
	last_motion_ = event;
}
QString lol_dashboard_visuals::distance_label() const
{
	double value = distance_ / 2800.0 * 2.54;
	QString unit = "cm";
	int decimals = value < 10.0 ? 2 : 1;
	if (value >= 100000.0) {
		value /= 100000.0;
		unit = "km";
		decimals = 3;
	} else if (value >= 1000.0) {
		value /= 100.0;
		unit = "m";
		decimals = 2;
	}
	return QString("%1 %2").arg(value, 0, 'f', decimals).arg(unit);
}
void lol_dashboard_visuals::draw_pointer(QPainter &painter, const QRect &bounds) const
{
	painter.setClipRect(bounds);
	const uint64_t now = os_gettime_ns();
	if (motion_trail_.size() > 1) {
		const auto map_point = [&](const QPointF &point) {
			return QPointF(bounds.left() + point.x() * bounds.width(),
				       bounds.top() + point.y() * bounds.height());
		};
		const auto opacity_for = [&](uint64_t time_ns) {
			return std::clamp(1.0 - qreal(now - time_ns) / qreal(mouse_trail_duration_ns), 0.0, 1.0);
		};
		for (size_t index = 1; index < motion_trail_.size(); ++index) {
			const auto &previous = motion_trail_[index - 1];
			const auto &sample = motion_trail_[index];
			const QPointF previous_point = map_point(previous.point);
			const QPointF sample_point = map_point(sample.point);
			const bool has_next = index + 1 < motion_trail_.size();
			const QPointF start = index == 1 ? previous_point : (previous_point + sample_point) / 2.0;
			const QPointF end = has_next ? (sample_point + map_point(motion_trail_[index + 1].point)) / 2.0
						     : sample_point;
			const uint64_t start_time = index == 1 ? previous.time_ns
							       : (previous.time_ns + sample.time_ns) / 2;
			const uint64_t end_time = has_next ? (sample.time_ns + motion_trail_[index + 1].time_ns) / 2
							   : sample.time_ns;
			QPainterPath path(start);
			path.quadTo(sample_point, end);
			QLinearGradient gradient(end, start);
			QColor newest(Qt::white);
			newest.setAlphaF(opacity_for(end_time));
			QColor oldest(Qt::white);
			oldest.setAlphaF(opacity_for(start_time));
			gradient.setColorAt(0.0, newest);
			gradient.setColorAt(1.0, oldest);
			painter.setPen(QPen(QBrush(gradient), 3, Qt::SolidLine, Qt::FlatCap, Qt::RoundJoin));
			painter.drawPath(path);
		}
		painter.setBrush(Qt::NoBrush);
	}
	for (size_t index = 1; index < trail_.size(); ++index) {
		const auto &previous = trail_[index - 1];
		const auto &event = trail_[index];
		const QPointF start(bounds.left() + previous.point.x() * bounds.width(),
				    bounds.top() + previous.point.y() * bounds.height());
		const QPointF end(bounds.left() + event.point.x() * bounds.width(),
				  bounds.top() + event.point.y() * bounds.height());
		QColor line(Qt::white);
		line.setAlphaF(std::pow(0.95, trail_.size() - index));
		painter.setPen(QPen(line, 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
		painter.setBrush(Qt::NoBrush);
		painter.drawLine(start, end);
	}
	for (size_t index = 0; index < trail_.size(); ++index) {
		const auto &event = trail_[index];
		const QPointF point(bounds.left() + event.point.x() * bounds.width(),
				    bounds.top() + event.point.y() * bounds.height());
		QColor color = event.button == MOUSE_BUTTON1   ? QColor(239, 68, 68)
			       : event.button == MOUSE_BUTTON2 ? QColor(59, 130, 246)
							       : QColor(250, 204, 21);
		color.setAlphaF(std::pow(0.95, trail_.size() - 1 - index));
		painter.setBrush(color);
		painter.setPen(Qt::NoPen);
		if (event.button == MOUSE_BUTTON1 || event.button == MOUSE_BUTTON2 || event.button == MOUSE_BUTTON3)
			painter.drawEllipse(point, 7, 7);
		else
			painter.drawRect(QRectF(point.x() - 7, point.y() - 7, 14, 14));
	}
	if (pointer_) {
		const QPointF point(bounds.left() + pointer_->x() * bounds.width(),
				    bounds.top() + pointer_->y() * bounds.height());
		painter.setBrush(Qt::white);
		painter.setPen(QPen(Qt::black, 2));
		painter.drawPolygon(QPolygonF{point, point + QPointF(0, 18), point + QPointF(5, 13),
					      point + QPointF(10, 20), point + QPointF(14, 18), point + QPointF(8, 11),
					      point + QPointF(15, 11)});
		for (size_t index = 0; index < pointer_indicators_.size(); ++index) {
			const auto &indicator = pointer_indicators_[index];
			const qreal opacity = indicator.fade_until
						      ? std::pow(std::clamp(qreal(indicator.fade_until - now) /
										    qreal(pointer_indicator_fade_ns),
									    0.0, 1.0),
								 2.0)
						      : 1.0;
			painter.save();
			painter.setOpacity(opacity);
			painter.setPen(Qt::white);
			painter.setFont(dashboard_font(style_.numbers_secondary, QFont::Bold));
			lol_dashboard_draw_shadowed_text(
				painter, QRect(int(point.x()) - 42, int(point.y()) - 24 - int(index) * 18, 38, 18),
				Qt::AlignRight | Qt::AlignVCenter, indicator.label);
			painter.restore();
		}
	}
}
namespace {
void draw_dashboard_value(QPainter &painter, const QRect &bounds, const sources::lol_dashboard_style &style,
			  const sources::lol_dashboard_theme &theme, const QString &label, const QString &value,
			  sources::lol_dashboard_alignment alignment)
{
	painter.setPen(Qt::white);
	const QRect content = bounds.adjusted(style.section_padding + style.element_padding,
					      style.section_padding + style.element_padding,
					      -style.section_padding - style.element_padding,
					      -style.section_padding - style.element_padding);
	const Qt::Alignment text_alignment = horizontal_alignment(alignment) | Qt::AlignVCenter;
	const int label_height = QFontMetrics(dashboard_font(style.number_labels, QFont::Bold)).height() + 2;
	const int value_height = QFontMetrics(dashboard_font(style.number_primary, QFont::Bold)).height() + 2;
	painter.setFont(dashboard_font(style.number_labels, QFont::Bold));
	lol_dashboard_draw_shadowed_text(painter, QRect(content.left(), content.top(), content.width(), label_height),
					 text_alignment, dashboard_text(label, style.number_labels));
	painter.setFont(dashboard_font(style.number_primary, QFont::Bold));
	painter.setPen(theme.active);
	lol_dashboard_draw_shadowed_text(painter,
					 QRect(content.left(), content.top() + label_height + style.within_element_gap,
					       content.width(), value_height),
					 text_alignment, dashboard_text(value, style.number_primary));
}
} // namespace

void lol_dashboard_visuals::draw_cumulative_totals(QPainter &painter, const QRect &bounds,
						   lol_dashboard_alignment alignment, int metric) const
{
	const std::array<QString, 3> labels{obs_module_text("MouseActivity.Clicks"),
					    obs_module_text("LoLPerformanceDashboard.KeyPresses"),
					    obs_module_text("MouseActivity.Distance")};
	const std::array<QString, 3> values{QString::number(total_clicks_), QString::number(total_key_presses_),
					    distance_label()};
	metric = std::clamp(metric, 0, 2);
	draw_dashboard_value(painter, bounds, style_, theme_, labels[metric], values[metric], alignment);
}

void lol_dashboard_visuals::draw_mouse_distance(QPainter &painter, const QRect &bounds,
						lol_dashboard_alignment alignment) const
{
	draw_dashboard_value(painter, bounds, style_, theme_, obs_module_text("MouseActivity.Distance"),
			     distance_label(), alignment);
}
#include "sources/dashboard/rendering/lol_keys.inc"
void lol_dashboard_visuals::draw_intensity(QPainter &painter, const QRect &bounds, int metric) const
{
	const QRect section = bounds.adjusted(style_.section_padding, style_.section_padding, -style_.section_padding,
					      -style_.section_padding);
	metric = std::clamp(metric, 0, 1);
	{
		const QRect card = section;
		const QRect content = card.adjusted(style_.element_padding, style_.element_padding,
						    -style_.element_padding, -style_.element_padding);
		if (content.width() < 1 || content.height() < 1)
			return;
		const int number_label_height = QFontMetrics(dashboard_font(style_.numbers_secondary)).height() + 2;
		const int label_height = QFontMetrics(dashboard_font(style_.number_labels, QFont::Bold)).height() + 2;
		const int number_height = QFontMetrics(dashboard_font(style_.number_primary, QFont::Bold)).height() + 2;
		const int text_gap = std::min(style_.within_element_gap, 4);
		std::vector<double> values;
		for (const auto &sample : session_samples_)
			values.push_back(metric == 0 ? sample[0] / 2800.0 * 2.54 : sample[1] * 60.0);
		std::array<double, 2> total = current_;
		for (const auto &sample : samples_) {
			total[metric] += sample[metric];
		}
		const double current = metric == 0 ? total[0] / window_ / 2800.0 * 2.54 : total[1] * 60.0 / window_;
		values.push_back(current);
		std::sort(values.begin(), values.end());
		const double min = values.front(), max = values.back(), range = std::max(0.001, max - min);
		const auto x = [&](double value) {
			return card.left() + 8 + int((value - min) / range * std::max(1, card.width() - 16));
		};
		const double q1 = values[(values.size() - 1) / 4], median = values[(values.size() - 1) / 2],
			     q3 = values[(values.size() - 1) * 3 / 4];
		const int y = content.top() + 8;
		painter.setPen(Qt::white);
		painter.drawLine(x(min), y, x(max), y);
		painter.setBrush(theme_.inactive);
		painter.drawRect(QRect(std::min(x(q1), x(q3)), y - 7, std::max(1, std::abs(x(q3) - x(q1))), 14));
		painter.drawLine(x(median), y - 8, x(median), y + 8);
		painter.setPen(QPen(theme_.active, 3));
		painter.drawLine(x(current), y - 13, x(current), y + 13);
		painter.setPen(Qt::white);
		painter.setFont(dashboard_font(style_.numbers_secondary));
		lol_dashboard_draw_shadowed_text(
			painter, QRect(content.left(), y + 14, content.width(), number_label_height), Qt::AlignLeft,
			dashboard_text(QString::number(min, 'f', min < 10 ? 1 : 0), style_.numbers_secondary));
		lol_dashboard_draw_shadowed_text(
			painter, QRect(content.left(), y + 14, content.width(), number_label_height), Qt::AlignRight,
			dashboard_text(QString::number(max, 'f', max < 10 ? 1 : 0), style_.numbers_secondary));
		const int label_top = y + 14 + number_label_height + text_gap;
		painter.setFont(dashboard_font(style_.number_labels, QFont::Bold));
		lol_dashboard_draw_shadowed_text(
			painter, QRect(content.left(), label_top, content.width(), label_height), Qt::AlignHCenter,
			dashboard_text(obs_module_text(metric ? "LoLPerformanceDashboard.APM"
							      : "LoLPerformanceDashboard.MouseVelocity"),
				       style_.number_labels));
		painter.setFont(dashboard_font(style_.number_primary, QFont::Bold));
		painter.setPen(theme_.active);
		lol_dashboard_draw_shadowed_text(painter,
						 QRect(content.left(), label_top + label_height + style_.label_spacing,
						       content.width(), number_height),
						 Qt::AlignHCenter,
						 dashboard_text(QString::number(current, 'f', current < 10 ? 1 : 0),
								style_.number_primary));
	}
}

void lol_dashboard_visuals::draw_widget(QPainter &painter, lol_dashboard_regions::widget widget, const QRect &bounds,
					int intensity_metric, int total_metric, lol_dashboard_alignment alignment) const
{
	if (bounds.isEmpty())
		return;
	switch (widget) {
	case lol_dashboard_regions::widget::intensity:
		draw_intensity(painter, bounds, intensity_metric);
		break;
	case lol_dashboard_regions::widget::mouse_activity:
		draw_pointer(painter, lol_dashboard_heatmap_content_bounds(bounds, game_frame_, style_));
		painter.setClipping(false);
		break;
	case lol_dashboard_regions::widget::cumulative_totals:
		draw_cumulative_totals(painter, bounds, alignment, total_metric);
		break;
	case lol_dashboard_regions::widget::mouse_distance:
		draw_mouse_distance(painter, bounds, alignment);
		break;
	case lol_dashboard_regions::widget::live_keys:
		draw_live_keys(painter, bounds, alignment);
		break;
	case lol_dashboard_regions::widget::top_keys:
		draw_top_keys(painter, bounds, alignment);
		break;
	case lol_dashboard_regions::widget::none:
		break;
	}
}

void lol_dashboard_visuals::draw(QPainter &painter, const std::array<QRect, 4> &top, const std::array<QRect, 4> &left,
				 const std::array<QRect, 4> &right) const
{
	ensure_dashboard_fonts_registered();
	const auto draw_section = [&](const lol_dashboard_regions::section &section,
				      const std::array<QRect, 4> &slot_rects, lol_dashboard_alignment alignment) {
		if (!section.enabled)
			return;
		for (int index = 0; index < std::clamp(section.count, 0, 4); ++index)
			draw_widget(painter, section.widgets[index], slot_rects[index],
				    section.intensity_metrics[index], section.total_metrics[index], alignment);
	};
	draw_section(regions_.top, top, lol_dashboard_alignment::center);
	draw_section(regions_.left, left, lol_dashboard_alignment::left);
	draw_section(regions_.right, right, lol_dashboard_alignment::right);
}

} // namespace sources
