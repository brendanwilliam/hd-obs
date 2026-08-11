#include "sources/dashboard/rendering/lol_visuals.hpp"

#include <QPainter>
#include <cmath>

namespace sources {

void lol_dashboard_draw_shadowed_text(QPainter &painter, const QRect &bounds, Qt::Alignment alignment,
				      const QString &text)
{
	const QPen pen = painter.pen();
	painter.setPen(Qt::black);
	painter.drawText(bounds.translated(2, 2), alignment, text);
	painter.setPen(pen);
	painter.drawText(bounds, alignment, text);
}

void lol_dashboard_visuals::reset()
{
	pointer_.reset();
	trail_.clear();
	last_distance_.reset();
	last_motion_.reset();
	held_.clear();
	press_counts_.clear();
	active_keys_.clear();
	samples_.clear();
	session_samples_.clear();
	current_.fill(0.0);
	bucket_start_ = total_clicks_ = 0;
	distance_ = 0.0;
}

} // namespace sources
