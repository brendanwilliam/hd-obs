#include "sources/game_report/collection/lol_game_context.hpp"

#include <cassert>

int main()
{
	using namespace sources::lol_game_report;
	const QJsonObject supported{
		{"activePlayer",
		 QJsonObject{{"riotIdGameName", "Player"}, {"riotIdTagLine", "NA1"}, {"championName", "Ahri"}}},
		{"gameData",
		 QJsonObject{{"mapNumber", 11}, {"gameMode", "CLASSIC"}, {"queueId", 420}, {"gameTime", 0.0}}}};
	assert(supported_game(parse_game_context(supported)));
	for (const auto &field : {QString("mapNumber"), QString("queueId")}) {
		QJsonObject mutated = supported;
		QJsonObject game = mutated["gameData"].toObject();
		game[field] = field == "mapNumber" ? 12 : 0;
		mutated["gameData"] = game;
		assert(!supported_game(parse_game_context(mutated)));
	}
	QJsonObject unsupported = supported;
	unsupported["gameData"] =
		QJsonObject{{"mapNumber", 11}, {"gameMode", "PRACTICETOOL"}, {"queueId", 420}, {"gameTime", 0.0}};
	assert(!supported_game(parse_game_context(unsupported)));
	return 0;
}
