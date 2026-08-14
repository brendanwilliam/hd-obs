#pragma once

#include <QJsonObject>
#include <QString>

namespace sources::lol_game_report {

struct game_context {
	QString riot_id_game_name;
	QString riot_id_tag_line;
	QString champion;
	QString game_mode;
	int map_number{};
	int queue_id{};
	double game_time{};
	bool game_end{};
};

game_context parse_game_context(const QJsonObject &all_game_data);
bool supported_game(const game_context &context);

} // namespace sources::lol_game_report
