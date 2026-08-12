void lol_dashboard_visuals::draw_live_keys(QPainter &painter, const QRect &bounds,
					   lol_dashboard_alignment alignment) const
{
	const QRect section = bounds.adjusted(style_.section_padding, style_.section_padding, -style_.section_padding,
					      -style_.section_padding);
	const QRect content = section.adjusted(style_.element_padding, style_.element_padding, -style_.element_padding,
					       -style_.element_padding);
	if (content.width() < 4 || content.height() < 4)
		return;
	painter.setPen(Qt::white);
	painter.setFont(dashboard_font(style_.number_labels, QFont::Bold));
	const int title_height = QFontMetrics(painter.font()).height() + 2;
	const int count_height = QFontMetrics(dashboard_font(style_.numbers_secondary)).height() + 2;
	const int active_row_height =
		std::max(style_.button_labels.size + count_height + style_.within_element_gap, 80);
	const int active_title_height = title_height;
	const int active_top = content.top();
	const Qt::Alignment edge = horizontal_alignment(alignment);
	lol_dashboard_draw_shadowed_text(painter,
					 QRect(content.left(), active_top, content.width(), active_title_height),
					 edge | Qt::AlignVCenter,
					 dashboard_text(obs_module_text("LiveKeys"), style_.number_labels));
	const int visible = std::min(4, int(active_keys_.size()));
	const int gap = style_.element_x_gap;
	const int key_width = std::max(1, (content.width() - (visible - 1) * gap) / 4);
	const QRect active_row(content.left(), active_top + active_title_height + style_.label_spacing, content.width(),
			       active_row_height);
	const uint64_t now = os_gettime_ns();
	for (int index = 0; index < visible; ++index) {
		const auto &key = active_keys_[active_keys_.size() - visible + index];
		const int x =
			alignment == lol_dashboard_alignment::right
				? active_row.right() - (visible - index) * key_width - (visible - index - 1) * gap + 1
			: alignment == lol_dashboard_alignment::center
				? active_row.left() +
					  (active_row.width() - (visible * key_width + (visible - 1) * gap)) / 2 +
					  index * (key_width + gap)
				: active_row.left() + index * (key_width + gap);
		QColor fill = held_.count(key.code) && held_.at(key.code) ? theme_.active : theme_.inactive;
		const qreal elapsed =
			key.fade_until
				? std::clamp(qreal(now - key.fade_started) / qreal(key.fade_until - key.fade_started),
					     0.0, 1.0)
				: 0.0;
		const qreal opacity = std::pow(1.0 - elapsed, 2.0);
		painter.save();
		painter.setOpacity(opacity);
		painter.setBrush(fill);
		painter.setPen(Qt::NoPen);
		const QRect key_rect(x, active_row.top() + count_height, key_width, active_row.height() - count_height);
		painter.drawRoundedRect(key_rect, 6, 6);
		painter.setPen(Qt::white);
		painter.setFont(dashboard_font(style_.numbers_secondary));
		lol_dashboard_draw_shadowed_text(
			painter, QRect(x, active_row.top(), key_width, count_height),
			Qt::AlignHCenter | Qt::AlignVCenter,
			QFontMetrics(painter.font())
				.elidedText(dashboard_text(QString::number(key.count), style_.numbers_secondary),
					    Qt::ElideLeft, key_width));
		painter.setFont(dashboard_font(style_.button_labels, QFont::Bold));
		lol_dashboard_draw_shadowed_text(painter, key_rect, Qt::AlignCenter,
						 QFontMetrics(painter.font())
							 .elidedText(dashboard_text(key.label, style_.button_labels),
								     Qt::ElideRight, key_rect.width()));
		painter.restore();
	}
}

void lol_dashboard_visuals::draw_top_keys(QPainter &painter, const QRect &bounds,
					  lol_dashboard_alignment alignment) const
{
	const QRect section = bounds.adjusted(style_.section_padding, style_.section_padding, -style_.section_padding,
					      -style_.section_padding);
	const QRect content = section.adjusted(style_.element_padding, style_.element_padding, -style_.element_padding,
					       -style_.element_padding);
	if (content.width() < 4 || content.height() < 4)
		return;
	painter.setPen(Qt::white);
	const Qt::Alignment edge = horizontal_alignment(alignment);
	std::vector<active_key> keys;
	for (const auto &[code, count] : press_counts_)
		keys.push_back({code, lol_dashboard_key_label(code), 0, 0, count});
	std::sort(keys.begin(), keys.end(), [](const auto &a, const auto &b) { return a.count > b.count; });
	if (keys.size() > 8)
		keys.resize(8);
	const uint64_t max =
		keys.empty() ? 1 : std::max_element(keys.begin(), keys.end(), [](const auto &a, const auto &b) {
					   return a.count < b.count;
				   })->count;
	const int chart_title_height = QFontMetrics(dashboard_font(style_.number_labels, QFont::Bold)).height() + 2;
	painter.setFont(dashboard_font(style_.number_labels, QFont::Bold));
	const int chart_top = content.top();
	lol_dashboard_draw_shadowed_text(
		painter, QRect(content.left(), chart_top, content.width(), chart_title_height), edge | Qt::AlignVCenter,
		dashboard_text(obs_module_text("LoLPerformanceDashboard.MostUsedKeys"), style_.number_labels));
	const QRect chart(content.left(), chart_top + chart_title_height + style_.label_spacing, content.width(),
			  std::max(1, content.bottom() - chart_top - chart_title_height - style_.label_spacing + 1));
	constexpr int bar_vertical_spacing = 12;
	const int row_height = std::max(1, chart.height() / 8);
	for (int index = 0; index < int(keys.size()); ++index) {
		const int y = chart.top() + index * (row_height + bar_vertical_spacing);
		const int text_height = QFontMetrics(dashboard_font(style_.numbers_secondary)).height() + 2;
		const int within_element_gap = style_.within_element_gap;
		if (alignment == lol_dashboard_alignment::left) {
			painter.setFont(dashboard_font(style_.numbers_secondary));
			const QFontMetrics metrics(painter.font());
			const QString label = dashboard_text(keys[index].label, style_.numbers_secondary);
			const QString count =
				dashboard_text(QString::number(keys[index].count), style_.numbers_secondary);
			const int count_width = metrics.horizontalAdvance(count) + 2;
			const int label_width = std::max(1, chart.width() / 4);
			const QRect label_rect(chart.left(), y, label_width, text_height);
			const QRect count_rect(chart.right() - count_width + 1, y, count_width, text_height);
			const QRect bar_bounds(
				label_rect.right() + within_element_gap + 1, y + text_height + 2,
				std::max(1, count_rect.left() - label_rect.right() - 2 * within_element_gap - 1), 12);
			const QRect bar_rect(bar_bounds.left(), bar_bounds.top(),
					     std::max(1, int(bar_bounds.width() * keys[index].count / max)),
					     bar_bounds.height());
			const auto pressed = held_.find(keys[index].code);
			painter.setBrush(pressed != held_.end() && pressed->second ? theme_.active : theme_.inactive);
			painter.setPen(Qt::NoPen);
			painter.drawRoundedRect(bar_rect, 3, 3);
			painter.setPen(Qt::white);
			lol_dashboard_draw_shadowed_text(painter, label_rect, Qt::AlignLeft | Qt::AlignVCenter,
							 metrics.elidedText(label, Qt::ElideRight, label_rect.width()));
			lol_dashboard_draw_shadowed_text(painter, count_rect, Qt::AlignRight | Qt::AlignVCenter,
							 metrics.elidedText(count, Qt::ElideLeft, count_rect.width()));
			continue;
		}
		const int bar = std::max(1, int(chart.width() * keys[index].count / max));
		const int bar_left = alignment == lol_dashboard_alignment::right ? chart.right() - bar + 1
				     : alignment == lol_dashboard_alignment::center
					     ? chart.left() + (chart.width() - bar) / 2
					     : chart.left();
		const QRect bar_rect(bar_left, y + text_height + 2, bar, 12);
		const auto pressed = held_.find(keys[index].code);
		painter.setBrush(pressed != held_.end() && pressed->second ? theme_.active : theme_.inactive);
		painter.setPen(Qt::NoPen);
		painter.drawRoundedRect(bar_rect, 3, 3);
		painter.setPen(Qt::white);
		painter.setFont(dashboard_font(style_.numbers_secondary));
		const QFontMetrics metrics(painter.font());
		const QString label = dashboard_text(keys[index].label, style_.numbers_secondary);
		const QString count = dashboard_text(QString::number(keys[index].count), style_.numbers_secondary);
		// QFontMetrics::elidedText can need a little more room than the advance
		// width because of glyph bearings.
		const int count_width = metrics.horizontalAdvance(count) + 2;
		const bool count_outside = metrics.horizontalAdvance(label) + within_element_gap + count_width > bar;
		const int label_width =
			count_outside ? std::max(0, std::min(metrics.horizontalAdvance(label),
							     chart.width() - within_element_gap - count_width))
				      : std::max(0, bar - within_element_gap - count_width);
		const bool right_aligned = alignment == lol_dashboard_alignment::right;
		const QRect label_rect(right_aligned ? bar_rect.right() - label_width + 1 : bar_rect.left(), y,
				       label_width, text_height);
		const QRect count_rect =
			count_outside
				? (right_aligned ? QRect(bar_rect.right() - label_width - within_element_gap -
								 count_width + 1,
							 y, count_width, text_height)
						 : QRect(bar_rect.left() + label_width + within_element_gap, y,
							 count_width, text_height))
				: (right_aligned
					   ? QRect(bar_rect.left(), y, count_width, text_height)
					   : QRect(bar_rect.right() - count_width + 1, y, count_width, text_height));
		if (label_width > 0)
			lol_dashboard_draw_shadowed_text(painter, label_rect,
							 horizontal_alignment(alignment) | Qt::AlignVCenter,
							 metrics.elidedText(label, Qt::ElideRight, label_rect.width()));
		lol_dashboard_draw_shadowed_text(painter, count_rect,
						 (right_aligned ? Qt::AlignLeft : Qt::AlignRight) | Qt::AlignVCenter,
						 metrics.elidedText(count, Qt::ElideLeft, count_rect.width()));
	}
}
