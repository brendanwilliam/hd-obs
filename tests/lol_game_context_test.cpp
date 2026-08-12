#include "sources/game_report/collection/lol_game_context.hpp"

#include <cassert>

int main()
{
	using namespace sources::lol_game_report;
	const QJsonObject supported{
		{"activePlayer",
		 QJsonObject{{"riotIdGameName", "Player"}, {"riotIdTagLine", "NA1"}, {"championName", "Ahri"}}},
		{"gameData", QJsonObject{{"mapNumber", 11},
					 {"gameMode", "CLASSIC"},
					 {"gameQueueConfigId", 420},
					 {"gameTime", 0.0}}}};
	assert(supported_game(parse_game_context(supported)));
	for (const auto &queue : {400, 420, 430, 440, 490}) {
		QJsonObject allowed = supported;
		QJsonObject game = allowed["gameData"].toObject();
		game["gameQueueConfigId"] = queue;
		allowed["gameData"] = game;
		assert(supported_game(parse_game_context(allowed)));
	}
	for (const auto &field : {QString("mapNumber"), QString("gameQueueConfigId")}) {
		QJsonObject mutated = supported;
		QJsonObject game = mutated["gameData"].toObject();
		game[field] = field == "mapNumber" ? 12 : 0;
		mutated["gameData"] = game;
		assert(!supported_game(parse_game_context(mutated)));
	}
	QJsonObject practice_tool = supported;
	practice_tool["gameData"] = QJsonObject{{"mapNumber", 11},
						{"gameMode", "PRACTICETOOL"},
						{"gameQueueConfigId", 0},
						{"gameTime", 0.0}};
	assert(supported_game(parse_game_context(practice_tool)));
	QJsonObject unsupported = supported;
	unsupported["gameData"] =
		QJsonObject{{"mapNumber", 11}, {"gameMode", "CLASSIC"}, {"gameQueueConfigId", 999}, {"gameTime", 0.0}};
	assert(!supported_game(parse_game_context(unsupported)));
	unsupported["gameData"] =
		QJsonObject{{"mapNumber", 11}, {"gameMode", "CLASSIC"}, {"queueId", 420}, {"gameTime", 0.0}};
	assert(!supported_game(parse_game_context(unsupported)));
	return 0;
}
