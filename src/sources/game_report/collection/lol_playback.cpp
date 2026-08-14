#include "sources/game_report/collection/lol_playback.hpp"

#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

namespace sources::lol_game_report {
namespace {
bool valid_pointer(const std::optional<QPointF> &pointer)
{
	return !pointer || (pointer->x() >= 0.0 && pointer->x() <= 1.0 &&
			    pointer->y() >= 0.0 && pointer->y() <= 1.0);
}
bool valid_action_label(const QString &label)
{
	static const QRegularExpression allowed(
		"^(?:(?:(?:self_cast|normal_cast|smart_cast|smart_plus_self_cast|"
		"smart_cast_with_indicator|smart_plus_self_cast_with_indicator)_)?"
		"(?:spell_[1-4]|summoner_[1-2]|item_[1-6]|trinket|role_bound)|"
		"recall|shop|attack_move|attack_move_click|attack_only_click|stop)$");
	return allowed.match(label).hasMatch();
}
bool has_only_keys(const QJsonObject &value, const QStringList &allowed)
{
	for (const QString &key : value.keys())
		if (!allowed.contains(key))
			return false;
	return true;
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
	    !valid_pointer(record.pointer) ||
	    (record.kind != playback_kind::gameplay_action && !record.pointer) ||
	    (!pointer_sample && record.kind != playback_kind::gameplay_action &&
	     !record.action_label.isEmpty()) ||
	    (record.kind == playback_kind::gameplay_action &&
	     !valid_action_label(record.action_label)))
		return false;
	if (pointer_sample &&
	    (stream.pointer_sample_count >= playback_max_pointer_samples ||
	     record.game_time_ms - stream.last_pointer_sample_ms <
		     playback_pointer_cadence_ms))
		return false;
	const qint64 record_bytes =
		QJsonDocument(record_json(record)).toJson(QJsonDocument::Compact).size();
	const qint64 array_bytes =
		stream.records.isEmpty()
			? record_bytes + 2
			: stream.canonical_array_bytes + record_bytes + 1;
	if (stream.records.size() >= playback_max_records ||
	    array_bytes > playback_max_serialized_bytes) {
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
	stream.canonical_array_bytes = array_bytes;
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

qint64 playback_canonical_array_bytes(const playback_stream &stream)
{
	return QJsonDocument(playback_json(stream)).toJson(QJsonDocument::Compact).size();
}

bool playback_from_json(const QJsonObject &value, playback_stream &stream)
{
	stream = {};
	if (!has_only_keys(value, {"records", "truncated", "omitted_record_count",
				   "timestamp_precision_ms"}) ||
	    !value["records"].isArray() || !value["truncated"].isBool() ||
	    !value["omitted_record_count"].isDouble() ||
	    !value["timestamp_precision_ms"].isDouble())
		return false;
	for (const auto item : value["records"].toArray()) {
		const QJsonObject record = item.toObject();
		if (!item.isObject() || !record["game_time_ms"].isDouble() ||
		    !record["kind"].isString())
			return false;
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
		const bool action = parsed == playback_kind::gameplay_action;
		const QStringList allowed =
			action ? QStringList{"game_time_ms", "kind", "pointer",
					     "action_label"}
			       : QStringList{"game_time_ms", "kind", "pointer"};
		if (!has_only_keys(record, allowed) ||
		    (action && !record["action_label"].isString()) ||
		    (!action && record.contains("action_label")))
			return false;
		std::optional<QPointF> pointer;
		if (record.contains("pointer")) {
			const QJsonObject point = record["pointer"].toObject();
			if (!record["pointer"].isObject() ||
			    !has_only_keys(point, {"x", "y"}) || !point["x"].isDouble() ||
			    !point["y"].isDouble())
				return false;
			pointer = QPointF{point["x"].toDouble(), point["y"].toDouble()};
		}
		if (!append_playback_record(stream,
					    {record["game_time_ms"].toInteger(), parsed,
					     pointer, record["action_label"].toString()}))
			return false;
	}
	stream.truncated = value["truncated"].toBool();
	stream.omitted_record_count = value["omitted_record_count"].toInt();
	return stream.omitted_record_count >= 0 &&
	       playback_canonical_array_bytes(stream) == stream.canonical_array_bytes;
}
} // namespace sources::lol_game_report
