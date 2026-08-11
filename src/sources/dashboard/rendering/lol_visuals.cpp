#include "sources/dashboard/rendering/lol_visuals.hpp"
#include "sources/dashboard/rendering/lol_key_labels.hpp"

#include <QFont>
#include <QFontDatabase>
#include <QFontMetrics>
#include <QPainter>
#include <QPolygonF>
#include <algorithm>
#include <cmath>
#include <obs-module.h>
#include <util/platform.h>

namespace sources {
namespace {
constexpr uint64_t second_ns = 1000000000ULL;
constexpr uint64_t live_key_fade_ns = 1500000000ULL;

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
} // namespace

void lol_dashboard_visuals::configure(const lol_dashboard_theme &theme, const lol_dashboard_regions &regions,
				      int rolling_window_seconds, const QRect &game_frame, const QRect &pointer_bounds,
				      const lol_dashboard_style &style)
{
	theme_ = theme;
	regions_ = regions;
	style_ = style;
	window_ = std::clamp(rolling_window_seconds, 1, 60);
	game_frame_ = game_frame;
	pointer_bounds_ = lol_dashboard_heatmap_content_bounds(pointer_bounds, game_frame_, style_);
}

QRect lol_dashboard_heatmap_content_bounds(const QRect &bounds, const QRect &game_frame,
					   const lol_dashboard_style &style)
{
	const int inset = style.section_padding + style.element_padding;
	const QRect content = bounds.adjusted(inset, inset, -inset, -inset);
	if (content.isEmpty() || game_frame.width() < 1 || game_frame.height() < 1)
		return {};
	const auto fitted = lol_dashboard_aspect_fit_left({content.x(), content.y(), content.width(), content.height()},
							  double(game_frame.width()) / game_frame.height());
	return {fitted.x(), fitted.y(), fitted.width(), fitted.height()};
}

void lol_dashboard_visuals::consume(const std::vector<input_data::trace_event> &events,
				    const input_data::button_map<uint16_t> &keyboard,
				    const input_data::button_map<uint16_t> &)
{
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
}

void lol_dashboard_visuals::clear_live_keys()
{
	held_.clear();
	active_keys_.clear();
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
	} else if (event.type == EVENT_KEY_RELEASED) {
		held_[event.code] = false;
		for (auto &key : active_keys_)
			if (key.code == event.code) {
				key.fade_started = event.time_ns;
				key.fade_until = event.time_ns + live_key_fade_ns;
			}
	} else if (event.type == EVENT_MOUSE_PRESSED) {
		++current_[1];
		++total_clicks_;
		if (game_frame_.contains(event.x, event.y)) {
			const QPointF point(double(event.x - game_frame_.left()) / std::max(1, game_frame_.width()),
					    double(event.y - game_frame_.top()) / std::max(1, game_frame_.height()));
			trail_.push_back({point, event.code});
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
	for (size_t index = 0; index < trail_.size(); ++index) {
		const auto &event = trail_[index];
		QColor color = event.button == MOUSE_BUTTON1   ? QColor(239, 68, 68)
			       : event.button == MOUSE_BUTTON2 ? QColor(59, 130, 246)
							       : QColor(250, 204, 21);
		color.setAlphaF(std::pow(0.95, trail_.size() - 1 - index));
		painter.setBrush(color);
		painter.setPen(Qt::NoPen);
		painter.drawEllipse(QPointF(bounds.left() + event.point.x() * bounds.width(),
					    bounds.top() + event.point.y() * bounds.height()),
				    7, 7);
	}
	if (pointer_) {
		painter.setBrush(Qt::white);
		painter.setPen(QPen(Qt::black, 2));
		painter.drawEllipse(QPointF(bounds.left() + pointer_->x() * bounds.width(),
					    bounds.top() + pointer_->y() * bounds.height()),
				    5, 5);
	}
}
void lol_dashboard_visuals::draw_summary(QPainter &painter, const QRect &bounds, bool right_aligned) const
{
	Q_UNUSED(right_aligned);
	painter.setPen(Qt::white);
	const QRect content = bounds.adjusted(style_.section_padding, style_.section_padding, -style_.section_padding,
					      -style_.section_padding);
	const int label_height = QFontMetrics(dashboard_font(style_.number_labels, QFont::Bold)).height() + 2;
	const int value_height = QFontMetrics(dashboard_font(style_.number_primary, QFont::Bold)).height() + 2;
	int top = content.top() + style_.element_padding;
	const auto alignment = Qt::AlignLeft | Qt::AlignVCenter;
	painter.setFont(dashboard_font(style_.number_labels, QFont::Bold));
	lol_dashboard_draw_shadowed_text(
		painter,
		QRect(content.left() + style_.element_padding, top, content.width() - 2 * style_.element_padding,
		      label_height),
		alignment, dashboard_text(obs_module_text("MouseActivity.Distance"), style_.number_labels));
	top += label_height + style_.label_spacing;
	painter.setFont(dashboard_font(style_.number_primary, QFont::Bold));
	painter.setPen(theme_.active);
	lol_dashboard_draw_shadowed_text(painter,
					 QRect(content.left() + style_.element_padding, top,
					       content.width() - 2 * style_.element_padding, value_height),
					 alignment, dashboard_text(distance_label(), style_.number_primary));
	top += value_height + style_.element_y_gap;
	painter.setPen(Qt::white);
	painter.setFont(dashboard_font(style_.number_labels, QFont::Bold));
	lol_dashboard_draw_shadowed_text(painter,
					 QRect(content.left() + style_.element_padding, top,
					       content.width() - 2 * style_.element_padding, label_height),
					 alignment,
					 dashboard_text(obs_module_text("MouseActivity.Clicks"), style_.number_labels));
	top += label_height + style_.label_spacing;
	painter.setFont(dashboard_font(style_.number_primary, QFont::Bold));
	painter.setPen(theme_.active);
	lol_dashboard_draw_shadowed_text(painter,
					 QRect(content.left() + style_.element_padding, top,
					       content.width() - 2 * style_.element_padding, value_height),
					 alignment,
					 dashboard_text(QString::number(total_clicks_), style_.number_primary));
}
#include "sources/dashboard/rendering/lol_keys.inc"
void lol_dashboard_visuals::draw_intensity(QPainter &painter, const QRect &bounds) const
{
	const QRect section = bounds.adjusted(style_.section_padding, style_.section_padding, -style_.section_padding,
					      -style_.section_padding);
	const int intensity_padding = std::min(style_.intensity_padding, std::max(0, (section.width() - 2) / 3));
	const int card_width = std::max(1, (section.width() - 3 * intensity_padding) / 2);
	for (int metric = 0; metric < 2; ++metric) {
		const QRect card(section.left() + intensity_padding + metric * (card_width + intensity_padding),
				 section.top(), card_width, section.height());
		const QRect content = card.adjusted(style_.element_padding, style_.element_padding,
						    -style_.element_padding, -style_.element_padding);
		if (content.width() < 1 || content.height() < 1)
			continue;
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

void lol_dashboard_visuals::draw(QPainter &painter, const QRect &header, const QRect &heatmap, const QRect &summary,
				 const QRect &keys, bool right_aligned) const
{
	ensure_dashboard_fonts_registered();
	painter.fillRect(QRect(0, 0, std::max({header.right(), heatmap.right(), summary.right(), keys.right()}) + 1,
			       std::max({header.bottom(), heatmap.bottom(), summary.bottom(), keys.bottom()}) + 1),
			 theme_.background);
	if (regions_.intensity)
		draw_intensity(painter, header);
	if (regions_.mouse_activity)
		draw_pointer(painter, lol_dashboard_heatmap_content_bounds(heatmap, game_frame_, style_));
	painter.setClipping(false);
	if (regions_.mouse_activity)
		draw_summary(painter, summary, right_aligned);
	if (regions_.keys)
		draw_keys(painter, keys, right_aligned);
}

} // namespace sources
