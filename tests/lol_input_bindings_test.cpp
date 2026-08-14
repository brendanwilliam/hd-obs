#include "sources/dashboard/detection/lol_input_bindings.hpp"
#include <cassert>
int main()
{
	using namespace sources;
	lol_input_bindings bindings;
	assert(bindings.parse(
		"[GameEvents]\nevtCastSpell1=[q]\nevtSelfCastSpell1=[Alt][q]\nevtCastAvatarSpell1=[d]\nevtNormalCastItem1=[Shift][1]\nevtSmartCastVisionItem=[Ctrl][Alt][4]\nevtOpenShop=[p],[n]\nevtUseItem7=[b]\nevtPlayerAttackMove=[a]\nevtPlayerAttackMoveClick=[Shift][a]\nevtPlayerAttackOnlyClick=[x]\nevtPlayerHoldPosition=[h]\nevtPlayerStopPosition=[s]\nevtUseItem1=[<Unbound>]\n[GameEvents.LeeSin]\nevtCastSpell1=[w]",
		"LeeSin"));
	assert(bindings.resolve("w", {}) && bindings.resolve("w", {})->action == "spell_1");
	assert(bindings.resolve("q", {"Alt"}) && bindings.resolve("q", {"Alt"})->action == "self_cast_spell_1");
	assert(bindings.resolve("d", {}) && bindings.resolve("d", {})->action == "summoner_1");
	assert(bindings.resolve("p", {}) && bindings.resolve("n", {}));
	assert(bindings.resolve("b", {}) && bindings.resolve("b", {})->action == "recall");
	assert(bindings.resolve("1", {"Shift"}) && bindings.resolve("1", {"Shift"})->action == "normal_cast_item_1");
	assert(bindings.resolve("4", {"Alt", "Ctrl"}) &&
	       bindings.resolve("4", {"Alt", "Ctrl"})->action == "smart_cast_trinket");
	assert(bindings.gameplay_actions().value("Alt+Ctrl+4") == "smart_cast_trinket");
	const auto actions = bindings.gameplay_actions();
	assert(actions.value("W") == "spell_1" && actions.value("D") == "summoner_1");
	assert(actions.value("B") == "recall" && actions.value("P") == "shop");
	assert(actions.value("Shift+1") == "normal_cast_item_1" && actions.value("Alt+Ctrl+4") == "smart_cast_trinket");
	assert(actions.value("A") == "attack_move" && actions.value("Shift+A") == "attack_move_click");
	assert(actions.value("X") == "attack_only_click" && actions.value("H") == "stop" &&
	       actions.value("S") == "stop");
	assert(!bindings.resolve("1", {}));
	assert(bindings.parse("[GameEvents]\nevtCastSpell1=[q]\n[GameEvents.Ahri]\nevtCastSpell1=[w]", "Ahri"));
	assert(bindings.resolve("w", {}) && !bindings.resolve("q", {}));
	assert(bindings.parse("[GameEvents]\nevtCastSpell1=[q]\n[GameEvents.Lux]\nevtCastSpell1=[e]", "Lux"));
	assert(bindings.resolve("e", {}) && !bindings.resolve("q", {}));
	return 0;
}
