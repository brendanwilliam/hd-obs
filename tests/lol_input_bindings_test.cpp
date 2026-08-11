#include "sources/dashboard/detection/lol_input_bindings.hpp"
#include <cassert>
int main()
{
	using namespace sources;
	lol_input_bindings bindings;
	assert(bindings.parse(
		"[GameEvents]\nevtCastSpell1=[q]\nevtSelfCastSpell1=[Alt][q]\nevtOpenShop=[p],[n]\nevtUseItem7=[b]\nevtUseItem1=[<Unbound>]\n[GameEvents.LeeSin]\nevtCastSpell1=[w]",
		"LeeSin"));
	assert(bindings.resolve("w", {}) && bindings.resolve("w", {})->action == "spell_1");
	assert(bindings.resolve("q", {"Alt"}) && bindings.resolve("q", {"Alt"})->action == "self_cast_spell_1");
	assert(bindings.resolve("p", {}) && bindings.resolve("n", {}));
	assert(bindings.resolve("b", {}) && bindings.resolve("b", {})->action == "recall");
	assert(!bindings.resolve("1", {}));
	return 0;
}
