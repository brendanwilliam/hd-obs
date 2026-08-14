#include "sources/game_report/data/lol_types.hpp"

#include <QJsonArray>
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QFile>
#include <cassert>

void run_playback_tests();

int main()
{
	using namespace sources::lol_game_report;
	report value;
	value.id = "a0f59d84-9d21-4d07-b903-2ec435ee0c1e";
	value.riot_id_game_name = "Player";
	value.riot_id_tag_line = "NA1";
	value.observed_started_at =
		QDateTime::fromString("2026-08-10T19:30:00Z", Qt::ISODate);
	value.completed_at = QDateTime::fromString("2026-08-10T19:32:00Z", Qt::ISODate);
	value.duration_seconds = 120;
	value.complete = true;
	value.v2_intensity = {{1, 20.0, 0.2}, {2, 40.0, 0.4}};
	value.v2_summary = {3, 4, 2, 40.0, 30.0, 0.4, 0.3};
	value.local_gameplay_events = QJsonArray{QJsonObject{{"sequence", "1"},
							     {"monotonic_time_ns", "100"},
							     {"action", "spell_1"},
							     {"chord", "Q"}}};
	const QJsonObject payload = to_json(value);
	assert(payload["schema_version"].toInt() == 2);
	assert(payload["report_id"] == value.id);
	assert(payload["capture"].toObject()["map_number"].toInt() == 11);
	assert(payload["capture"].toObject()["riot_id"].toObject()["game_name"] ==
	       "Player");
	assert(!payload.contains("events") && !payload.contains("hexbins") &&
	       !payload.contains("raw_keys"));
	assert(!payload.contains("local_gameplay_events"));
	const QJsonObject input = payload["input"].toObject();
	assert(input["intensity_by_second"].toArray().size() == 2);
	assert(input["summary"].toObject()["peak_apm"].toDouble() == 40.0);
	assert(input["left_clicks"].toInt() == 3);
	assert(input["gameplay_key_actions"].toInt() == 2);
	value.game_mode = "PRACTICETOOL";
	assert(to_json(value)["capture"].toObject()["game_mode"] == "PRACTICETOOL");
	value.complete = false;
	assert(!to_json(value)["capture"].toObject()["complete"].toBool());
	value.playback.records.append(
		{1'000, playback_kind::left_click, QPointF{0.25, 0.75}});
	const QJsonObject v3 = to_json(value);
	assert(v3["schema_version"].toInt() == 3);
	const QJsonObject playback = v3["input"].toObject()["playback"].toObject();
	assert(playback["records"].toArray().first().toObject()["kind"] == "left_click");
	assert(!QJsonDocument(v3).toJson().contains("monotonic_time_ns"));
	report restored;
	assert(from_json(v3, restored));
	assert(restored.playback.records.size() == 1);
	QJsonObject reordered{{"z", 1}, {"a", QJsonObject{{"z", 1}, {"a", 2}}}};
	QJsonObject ordered{{"a", QJsonObject{{"a", 2}, {"z", 1}}}, {"z", 1}};
	assert(canonical_payload(reordered) == canonical_payload(ordered));
	QFile fixture(QStringLiteral(HD_OBS_SOURCE_DIR
				     "/tests/fixtures/hands-diff/v3-playback.json"));
	assert(fixture.open(QIODevice::ReadOnly));
	const QJsonObject fixture_payload =
		QJsonDocument::fromJson(fixture.readAll()).object();
	assert(fixture_payload["payload_hash"].isString());
	assert(QString::fromLatin1(
		       QCryptographicHash::hash(canonical_payload(fixture_payload),
						QCryptographicHash::Sha256)
			       .toHex()) == fixture_payload["payload_hash"].toString());
	report fixture_report;
	assert(from_json(fixture_payload, fixture_report));
	assert(fixture_report.playback.records.size() == 4);
	run_playback_tests();
	return 0;
}
