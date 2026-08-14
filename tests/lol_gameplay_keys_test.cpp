#include "sources/game_report/collection/lol_gameplay_keys.hpp"

#include "input/keycodes.h"

#include <cassert>

int main()
{
	using namespace sources::lol_game_report;
	assert(gameplay_key_name(VC_Q) == "Q");
	assert(gameplay_key_name(VC_LEFT) == "Left Arrow");
	assert(gameplay_modifier_name(VC_ALT_L) == "Alt");
	assert(gameplay_chord({"Ctrl", "Alt"}, VC_Q) == "Alt+Ctrl+Q");
	assert(gameplay_chord({}, VC_SHIFT_L).isEmpty());
	return 0;
}
