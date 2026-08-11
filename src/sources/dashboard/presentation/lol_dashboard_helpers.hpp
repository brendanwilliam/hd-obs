#pragma once

#include "sources/dashboard/rendering/lol_layout.hpp"

#include <QColor>
#include <QRect>

namespace sources {

inline QColor lol_dashboard_obs_color(uint32_t value)
{
	return {int(value & 0xff), int((value >> 8) & 0xff), int((value >> 16) & 0xff), int((value >> 24) & 0xff)};
}

inline QRect lol_dashboard_qrect(const lol_dashboard_rect &rect)
{
	return {rect.x(), rect.y(), rect.width(), rect.height()};
}

} // namespace sources
