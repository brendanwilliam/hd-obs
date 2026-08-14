#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QPointF>
#include <QString>
#include <QVector>

#include <optional>

namespace sources::lol_game_report {

enum class playback_kind {
	pointer_sample,
	left_click,
	right_click,
	middle_click,
	gameplay_action
};

struct playback_record {
	qint64 game_time_ms{};
	playback_kind kind{playback_kind::pointer_sample};
	std::optional<QPointF> pointer;
	QString action_label;
};

struct playback_stream {
	QVector<playback_record> records;
	int omitted_record_count{};
	bool truncated{};
	qint64 last_pointer_sample_ms{-100};
	int pointer_sample_count{};
	qint64 serialized_bytes{};
};

constexpr qint64 playback_max_game_time_ms = 60 * 60 * 1000;
constexpr int playback_max_pointer_samples = 36'000;
constexpr int playback_max_records = 50'000;
constexpr qint64 playback_max_serialized_bytes = 5 * 1024 * 1024;
constexpr qint64 playback_pointer_cadence_ms = 100;

bool append_playback_record(playback_stream &stream, playback_record record);
QJsonArray playback_json(const playback_stream &stream);
bool playback_from_json(const QJsonObject &value, playback_stream &stream);
QString playback_kind_name(playback_kind kind);

} // namespace sources::lol_game_report
