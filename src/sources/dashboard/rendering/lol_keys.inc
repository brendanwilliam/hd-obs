void lol_dashboard_visuals::draw_live_keys(QPainter &painter, const QRect &bounds,
					   lol_dashboard_alignment alignment) const
{
	const QRect section =
		bounds.adjusted(style_.section_padding, style_.section_padding,
				-style_.section_padding, -style_.section_padding);
	const QRect content =
		section.adjusted(style_.element_padding, style_.element_padding,
				 -style_.element_padding, -style_.element_padding);
	if (content.width() < 4 || content.height() < 4)
		return;
	painter.setPen(Qt::white);
	painter.setFont(dashboard_font(style_.number_labels, QFont::Bold));
	const int title_height = QFontMetrics(painter.font()).height() + 2;
	const int count_height =
		QFontMetrics(dashboard_font(style_.numbers_secondary)).height() + 2;
	const int active_row_height = std::max(
		style_.button_labels.size + count_height + style_.within_element_gap, 80);
	const int active_title_height = title_height;
	const int active_top = content.top();
	const Qt::Alignment edge = horizontal_alignment(alignment);
	lol_dashboard_draw_shadowed_text(
		painter,
		QRect(content.left(), active_top, content.width(), active_title_height),
		edge | Qt::AlignVCenter,
		dashboard_text(obs_module_text("LiveKeys"), style_.number_labels));
	const int visible = std::min(4, int(active_keys_.size()));
	const int gap = style_.element_x_gap;
	const int key_width = std::max(1, (content.width() - (visible - 1) * gap) / 4);
	const QRect active_row(content.left(),
			       active_top + active_title_height + style_.label_spacing,
			       content.width(), active_row_height);
	const int row_width = visible * key_width + std::max(0, visible - 1) * gap;
	const int row_left =
		alignment == lol_dashboard_alignment::right
			? active_row.right() - row_width + 1
		: alignment == lol_dashboard_alignment::center
			? active_row.left() + (active_row.width() - row_width) / 2
			: active_row.left();
	const uint64_t now = os_gettime_ns();
	for (int index = 0; index < visible; ++index) {
		const int key_index = alignment == lol_dashboard_alignment::left
					      ? visible - 1 - index
					      : index;
		const auto &key = active_keys_[active_keys_.size() - visible + key_index];
		const int x = row_left + index * (key_width + gap);
		QColor fill = held_.count(key.code) && held_.at(key.code)
				      ? theme_.active
				      : theme_.inactive;
		const qreal elapsed =
			key.fade_until ? std::clamp(qreal(now - key.fade_started) /
							    qreal(key.fade_until -
								  key.fade_started),
						    0.0, 1.0)
				       : 0.0;
		const qreal opacity = std::pow(1.0 - elapsed, 2.0);
		painter.save();
		painter.setOpacity(opacity);
		painter.setBrush(fill);
		painter.setPen(Qt::NoPen);
		const QRect key_rect(x, active_row.top() + count_height, key_width,
				     active_row.height() - count_height);
		painter.drawRoundedRect(key_rect, 6, 6);
		painter.setPen(Qt::white);
		painter.setFont(dashboard_font(style_.numbers_secondary));
		lol_dashboard_draw_shadowed_text(
			painter, QRect(x, active_row.top(), key_width, count_height),
			Qt::AlignHCenter | Qt::AlignVCenter,
			QFontMetrics(painter.font())
				.elidedText(dashboard_text(QString::number(key.count),
							   style_.numbers_secondary),
					    Qt::ElideLeft, key_width));
		painter.setFont(dashboard_font(style_.button_labels, QFont::Bold));
		lol_dashboard_draw_shadowed_text(
			painter, key_rect, Qt::AlignCenter,
			QFontMetrics(painter.font())
				.elidedText(dashboard_text(key.label,
							   style_.button_labels),
					    Qt::ElideRight, key_rect.width()));
		painter.restore();
	}
}

void lol_dashboard_visuals::draw_top_keys(QPainter &painter, const QRect &bounds,
					  lol_dashboard_alignment alignment) const
{
	const QRect section =
		bounds.adjusted(style_.section_padding, style_.section_padding,
				-style_.section_padding, -style_.section_padding);
	const QRect content =
		section.adjusted(style_.element_padding, style_.element_padding,
				 -style_.element_padding, -style_.element_padding);
	if (content.width() < 4 || content.height() < 4)
		return;
	painter.setPen(Qt::white);
	const Qt::Alignment edge = horizontal_alignment(alignment);
	std::vector<active_key> keys;
	for (const auto &[code, count] : press_counts_)
		keys.push_back({code, lol_dashboard_key_label(code), 0, 0, count});
	std::sort(keys.begin(), keys.end(),
		  [](const auto &a, const auto &b) { return a.count > b.count; });
	if (keys.size() > 8)
		keys.resize(8);
	const uint64_t max = keys.empty()
				     ? 1
				     : std::max_element(keys.begin(), keys.end(),
							[](const auto &a, const auto &b) {
								return a.count < b.count;
							})
					       ->count;
	const int chart_title_height =
		QFontMetrics(dashboard_font(style_.number_labels, QFont::Bold)).height() +
		2;
	painter.setFont(dashboard_font(style_.number_labels, QFont::Bold));
	const int chart_top = content.top();
	lol_dashboard_draw_shadowed_text(
		painter,
		QRect(content.left(), chart_top, content.width(), chart_title_height),
		edge | Qt::AlignVCenter,
		dashboard_text(obs_module_text("LoLPerformanceDashboard.MostUsedKeys"),
			       style_.number_labels));
	const QRect chart(content.left(),
			  chart_top + chart_title_height + style_.label_spacing,
			  content.width(),
			  std::max(1, content.bottom() - chart_top - chart_title_height -
					      style_.label_spacing + 1));
	constexpr int bar_vertical_spacing = 8;
	const int text_height =
		QFontMetrics(dashboard_font(style_.numbers_secondary)).height() + 2;
	const int minimum_row_height = std::max(text_height, 12);
	const int visible_rows = std::min(
		int(keys.size()),
		std::max(1, (chart.height() + bar_vertical_spacing) /
				    (minimum_row_height + bar_vertical_spacing)));
	const int row_height = minimum_row_height;
	painter.setFont(dashboard_font(style_.numbers_secondary));
	const QFontMetrics metrics(painter.font());
	int label_width = 1, count_width = 1;
	for (const auto &key : keys) {
		label_width = std::max(label_width,
				       metrics.horizontalAdvance(dashboard_text(
					       key.label, style_.numbers_secondary)));
		count_width = std::max(count_width, metrics.horizontalAdvance(
							    QString::number(key.count)));
	}
	label_width = std::min(label_width + 2, std::max(1, chart.width() / 3));
	count_width += 2;
	const int gap = style_.within_element_gap;
	const bool right_aligned = alignment == lol_dashboard_alignment::right;
	for (int index = 0; index < visible_rows; ++index) {
		const int y = chart.top() + index * (row_height + bar_vertical_spacing);
		const QRect label_rect(right_aligned ? chart.right() - label_width + 1
						     : chart.left(),
				       y, label_width, row_height);
		const int bar_left = right_aligned ? chart.left() + count_width + gap
						   : label_rect.right() + gap + 1;
		const int bar_right = right_aligned ? label_rect.left() - gap - 1
						    : chart.right() - count_width - gap;
		const int bar_width = std::max(1, bar_right - bar_left + 1);
		const int bar = std::max(1, int(bar_width * keys[index].count / max));
		const QRect bar_rect(right_aligned ? bar_right - bar + 1 : bar_left,
				     y + (row_height - 12) / 2, bar, 12);
		const QRect count_rect(right_aligned ? bar_rect.left() - gap - count_width
						     : bar_rect.right() + gap + 1,
				       y, count_width, row_height);
		const auto pressed = held_.find(keys[index].code);
		painter.setBrush(pressed != held_.end() && pressed->second
					 ? theme_.active
					 : theme_.inactive);
		painter.setPen(Qt::NoPen);
		painter.drawRoundedRect(bar_rect, 3, 3);
		painter.setPen(Qt::white);
		const QString label =
			dashboard_text(keys[index].label, style_.numbers_secondary);
		const QString count = dashboard_text(QString::number(keys[index].count),
						     style_.numbers_secondary);
		lol_dashboard_draw_shadowed_text(
			painter, label_rect, Qt::AlignHCenter | Qt::AlignVCenter,
			metrics.elidedText(label, Qt::ElideRight, label_rect.width()));
		lol_dashboard_draw_shadowed_text(
			painter, count_rect,
			(right_aligned ? Qt::AlignLeft : Qt::AlignRight) |
				Qt::AlignVCenter,
			metrics.elidedText(count, Qt::ElideLeft, count_rect.width()));
	}
}
