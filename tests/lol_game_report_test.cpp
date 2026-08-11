#include "sources/game_report/data/lol_types.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <cassert>

int main()
{
	using namespace sources::lol_game_report;
	report value;
	value.id = "a0f59d84-9d21-4d07-b903-2ec435ee0c1e";
	value.player = "Player#NA1";
	value.completed_at = QDateTime::fromString("2026-08-10T19:32:00Z", Qt::ISODate);
	value.duration_seconds = 120;
	value.input_samples = {{1, 1, 0, 0.2}, {2, 2, 0, 0.4}};
	const QJsonObject payload = to_json(value);
	assert(payload["schema_version"].toInt() == 2);
	assert(payload["report_id"] == value.id);
	assert(payload["capture"].toObject()["map_number"].toInt() == 11);
	assert(payload["capture"].toObject()["riot_id"].toObject()["game_name"] == "Player");
	assert(!payload.contains("events") && !payload.contains("hexbins") && !payload.contains("raw_keys"));
	const QJsonObject input = payload["input"].toObject();
	assert(input["intensity_by_second"].toArray().size() == 2);
	assert(input["summary"].toObject()["peak_apm"].toDouble() == 40.0);
	return 0;
}
