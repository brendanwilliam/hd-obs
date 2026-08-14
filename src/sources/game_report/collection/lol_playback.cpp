#include "sources/game_report/collection/lol_playback.hpp"

#include <QJsonDocument>
#include <QJsonObject>

namespace sources::lol_game_report {
namespace {
bool valid_pointer(const std::optional<QPointF> &pointer)
{
	return !pointer || (pointer->x() >= 0.0 && pointer->x() <= 1.0 &&
			    pointer->y() >= 0.0 && pointer->y() <= 1.0);
}
QJsonObject record_json(const playback_record &record)
{
	QJsonObject value{{"game_time_ms", record.game_time_ms},
			  {"kind", playback_kind_name(record.kind)}};
	if (record.pointer)
		value.insert("pointer", QJsonObject{{"x", record.pointer->x()},
						    {"y", record.pointer->y()}});
	if (!record.action_label.isEmpty())
		value.insert("action_label", record.action_label);
	return value;
}
} // namespace

QString playback_kind_name(playback_kind kind)
{
	if (kind == playback_kind::left_click)
		return "left_click";
	if (kind == playback_kind::right_click)
		return "right_click";
	if (kind == playback_kind::middle_click)
		return "middle_click";
	if (kind == playback_kind::gameplay_action)
		return "gameplay_action";
	return "pointer_sample";
}

bool append_playback_record(playback_stream &stream, playback_record record)
{
	const bool pointer_sample = record.kind == playback_kind::pointer_sample;
	if (record.game_time_ms < 0 || record.game_time_ms > playback_max_game_time_ms ||
	    !valid_pointer(record.pointer) || (pointer_sample && !record.pointer) ||
	    (!pointer_sample && record.kind != playback_kind::gameplay_action &&
	     !record.action_label.isEmpty()) ||
	    (record.kind == playback_kind::gameplay_action &&
	     record.action_label.isEmpty()))
		return false;
	if (pointer_sample &&
	    (stream.pointer_sample_count >= playback_max_pointer_samples ||
	     record.game_time_ms - stream.last_pointer_sample_ms <
		     playback_pointer_cadence_ms))
		return false;
	const qint64 bytes =
		QJsonDocument(record_json(record)).toJson(QJsonDocument::Compact).size();
	if (stream.records.size() >= playback_max_records ||
	    stream.serialized_bytes + bytes > playback_max_serialized_bytes) {
		stream.truncated = true;
		++stream.omitted_record_count;
		return false;
	}
	if (!stream.records.isEmpty() &&
	    record.game_time_ms <= stream.records.last().game_time_ms) {
		stream.truncated = true;
		++stream.omitted_record_count;
		return false;
	}
	stream.records.append(std::move(record));
	stream.serialized_bytes += bytes;
	if (pointer_sample) {
		++stream.pointer_sample_count;
		stream.last_pointer_sample_ms = stream.records.last().game_time_ms;
	}
	return true;
}

QJsonArray playback_json(const playback_stream &stream)
{
	QJsonArray result;
	for (const auto &record : stream.records)
		result.append(record_json(record));
	return result;
}

bool playback_from_json(const QJsonObject &value, playback_stream &stream)
{
	stream = {};
	for (const auto item : value["records"].toArray()) {
		const QJsonObject record = item.toObject();
		const QString kind = record["kind"].toString();
		playback_kind parsed = playback_kind::pointer_sample;
		if (kind == "left_click")
			parsed = playback_kind::left_click;
		else if (kind == "right_click")
			parsed = playback_kind::right_click;
		else if (kind == "middle_click")
			parsed = playback_kind::middle_click;
		else if (kind == "gameplay_action")
			parsed = playback_kind::gameplay_action;
		else if (kind != "pointer_sample")
			return false;
		std::optional<QPointF> pointer;
		if (record.contains("pointer")) {
			const QJsonObject point = record["pointer"].toObject();
			pointer = QPointF{point["x"].toDouble(), point["y"].toDouble()};
		}
		if (!append_playback_record(stream,
					    {record["game_time_ms"].toInteger(), parsed,
					     pointer, record["action_label"].toString()}))
			return false;
	}
	stream.truncated = value["truncated"].toBool();
	stream.omitted_record_count = value["omitted_record_count"].toInt();
	return stream.omitted_record_count >= 0;
}
} // namespace sources::lol_game_report
