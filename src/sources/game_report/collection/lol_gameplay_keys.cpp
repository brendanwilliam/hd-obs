#include "sources/game_report/collection/lol_gameplay_keys.hpp"

#include "input/keycodes.h"

#include <algorithm>

namespace sources::lol_game_report {
QString gameplay_key_name(uint16_t code)
{
	if (code >= VC_A && code <= VC_Z)
		return QString(QChar('A' + code - VC_A));
	if (code >= VC_0 && code <= VC_9)
		return QString(QChar('0' + code - VC_0));
	if (code >= VC_F1 && code <= VC_F12)
		return QString("F%1").arg(code - VC_F1 + 1);
	switch (code) {
	case VC_LEFT:
		return "Left Arrow";
	case VC_RIGHT:
		return "Right Arrow";
	case VC_UP:
		return "Up Arrow";
	case VC_DOWN:
		return "Down Arrow";
	case VC_SPACE:
		return "Space";
	case VC_TAB:
		return "Tab";
	case VC_ENTER:
		return "Return";
	case VC_ESCAPE:
		return "Esc";
	case VC_BACK_QUOTE:
		return "`";
	default:
		return {};
	}
}

QString gameplay_modifier_name(uint16_t code)
{
	switch (code) {
	case VC_SHIFT_L:
	case VC_SHIFT_R:
		return "Shift";
	case VC_CONTROL_L:
	case VC_CONTROL_R:
		return "Ctrl";
	case VC_ALT_L:
	case VC_ALT_R:
		return "Alt";
	case VC_META_L:
	case VC_META_R:
		return "Cmd";
	default:
		return {};
	}
}

QString gameplay_chord(QSet<QString> modifiers, uint16_t trigger)
{
	const QString key = gameplay_key_name(trigger);
	if (key.isEmpty())
		return {};
	QStringList parts = modifiers.values();
	std::sort(parts.begin(), parts.end());
	parts.append(key);
	return parts.join('+');
}

} // namespace sources::lol_game_report
