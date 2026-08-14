#include "sources/game_report/collection/lol_game_context.hpp"

#include <cassert>

int main()
{
	using namespace sources::lol_game_report;
	const QJsonObject supported{
		{"activePlayer",
		 QJsonObject{{"riotIdGameName", "Player"}, {"riotIdTagLine", "NA1"}, {"championName", "Ahri"}}},
		{"gameData", QJsonObject{{"mapNumber", 11}, {"gameMode", "CLASSIC"}, {"gameTime", 0.0}}}};
	assert(supported_game(parse_game_context(supported)));
	QJsonObject unsupported_map = supported;
	QJsonObject game = unsupported_map["gameData"].toObject();
	game["mapNumber"] = 12;
	unsupported_map["gameData"] = game;
	assert(!supported_game(parse_game_context(unsupported_map)));
	QJsonObject practice_tool = supported;
	practice_tool["gameData"] = QJsonObject{{"mapNumber", 11},
						{"gameMode", "PRACTICETOOL"},
						{"gameQueueConfigId", 0},
						{"gameTime", 0.0}};
	assert(supported_game(parse_game_context(practice_tool)));
	return 0;
}
