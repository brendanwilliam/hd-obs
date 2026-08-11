#include "sources/game_report/collection/lol_game_context.hpp"

#include <QJsonArray>

namespace sources::lol_game_report {
namespace {
int queue_id(const QJsonObject &game)
{
	for (const char *key : {"queueId", "gameQueue", "queueID"}) {
		const int value = game[key].toInt();
		if (value > 0)
			return value;
	}
	return 0;
}
} // namespace

game_context parse_game_context(const QJsonObject &all_game_data)
{
	game_context result;
	const QJsonObject player = all_game_data["activePlayer"].toObject();
	const QJsonObject game = all_game_data["gameData"].toObject();
	result.riot_id_game_name = player["riotIdGameName"].toString();
	result.riot_id_tag_line = player["riotIdTagLine"].toString();
	result.champion = player["championName"].toString();
	result.game_mode = game["gameMode"].toString();
	result.map_number = game["mapNumber"].toInt();
	result.queue_id = queue_id(game);
	result.game_time = game["gameTime"].toDouble(-1.0);
	const QJsonArray events = all_game_data["events"].toObject()["Events"].toArray();
	for (const QJsonValue value : events)
		result.game_end = result.game_end || value.toObject()["EventName"].toString() == "GameEnd";
	return result;
}

bool supported_game(const game_context &context)
{
	return !context.riot_id_game_name.isEmpty() && !context.riot_id_tag_line.isEmpty() &&
	       context.map_number == 11 && context.game_mode == "CLASSIC" &&
	       (context.queue_id == 400 || context.queue_id == 420 || context.queue_id == 430 ||
		context.queue_id == 440 || context.queue_id == 490) &&
	       context.game_time >= 0.0;
}

} // namespace sources::lol_game_report
