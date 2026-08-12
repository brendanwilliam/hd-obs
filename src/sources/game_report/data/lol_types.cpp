#include "sources/game_report/data/lol_types.hpp"

#include <QJsonArray>
#include <algorithm>
#include <numeric>

namespace sources::lol_game_report {
namespace {
QJsonObject sample_json(const stat_sample &s)
{
	return {{"seconds", s.seconds},
		{"kills", s.kills},
		{"deaths", s.deaths},
		{"assists", s.assists},
		{"cs", s.cs},
		{"level", s.level},
		{"gold", s.gold},
		{"ward_score", s.ward_score},
		{"estimated_gold", s.estimated_gold}};
}
stat_sample sample_from_json(const QJsonObject &o)
{
	return {o["seconds"].toInt(), o["kills"].toInt(),      o["deaths"].toInt(),
		o["assists"].toInt(), o["cs"].toInt(),         o["level"].toInt(),
		o["gold"].toInt(),    o["ward_score"].toInt(), o["estimated_gold"].toInt(o["gold"].toInt())};
}
QJsonArray strings_json(const QStringList &values)
{
	QJsonArray result;
	for (const auto &value : values)
		result.append(value);
	return result;
}
} // namespace

QJsonObject to_json(const report &v)
{
	QString game_name = v.riot_id_game_name, tag_line = v.riot_id_tag_line;
	if (game_name.isEmpty())
		game_name = v.player;
	const qsizetype separator = game_name.lastIndexOf('#');
	if (separator > 0) {
		if (tag_line.isEmpty())
			tag_line = game_name.mid(separator + 1);
		game_name.truncate(separator);
	}
	if (tag_line.isEmpty())
		tag_line = "unknown";
	QJsonArray intensity;
	QVector<intensity_sample> samples = v.v2_intensity;
	for (const auto &sample : samples)
		intensity.append(QJsonObject{{"second", sample.second},
					     {"apm", sample.apm},
					     {"mouse_velocity", sample.mouse_velocity}});
	metric_summary summary = v.v2_summary;
	if (v.v2_intensity.isEmpty()) {
		QVector<double> apm, velocity;
		for (const auto &sample : samples) {
			apm.append(sample.apm);
			velocity.append(sample.mouse_velocity);
			summary.peak_apm = std::max(summary.peak_apm, sample.apm);
			summary.peak_mouse_velocity = std::max(summary.peak_mouse_velocity, sample.mouse_velocity);
		}
		auto median = [](QVector<double> values) {
			if (values.isEmpty())
				return 0.0;
			std::sort(values.begin(), values.end());
			const qsizetype middle = values.size() / 2;
			return values.size() % 2 ? values[middle] : (values[middle - 1] + values[middle]) / 2.0;
		};
		summary.median_apm = median(apm);
		summary.median_mouse_velocity = median(velocity);
	}
	const int duration_ms = v.duration_seconds > 0 ? v.duration_seconds * 1000
						       : int(v.observed_started_at.msecsTo(v.completed_at));
	const QDateTime started = v.observed_started_at.isValid() ? v.observed_started_at
								  : v.completed_at.addMSecs(-duration_ms);
	return {{"schema_version", 2},
		{"report_id", v.id},
		{"capture_policy_version", 1},
		{"capture", QJsonObject{{"started_at_utc", started.toUTC().toString(Qt::ISODateWithMs)},
					{"duration_ms", duration_ms},
					{"game_mode", v.game_mode},
					{"map_number", v.map_number},
					{"riot_id", QJsonObject{{"game_name", game_name}, {"tag_line", tag_line}}},
					{"frontmost_capture", v.frontmost_capture},
					{"complete", v.complete},
					{"event_detail_truncated", v.event_detail_truncated}}},
		{"input",
		 QJsonObject{{"left_clicks", summary.left_clicks},
			     {"right_clicks", summary.right_clicks},
			     {"gameplay_key_actions", summary.gameplay_key_actions},
			     {"intensity_by_second", intensity},
			     {"summary", QJsonObject{{"peak_apm", summary.peak_apm},
						     {"median_apm", summary.median_apm},
						     {"peak_mouse_velocity", summary.peak_mouse_velocity},
						     {"median_mouse_velocity", summary.median_mouse_velocity}}}}},
		{"live_context", QJsonObject{{"changes", QJsonArray{}}}}};
}

bool from_json(const QJsonObject &o, report &v)
{
	if (o["schema_version"].toInt() != 2 || o["report_id"].toString().isEmpty())
		return false;
	v = {};
	v.id = o["report_id"].toString();
	const QJsonObject capture = o["capture"].toObject();
	const QJsonObject riot_id = capture["riot_id"].toObject();
	v.riot_id_game_name = riot_id["game_name"].toString();
	v.riot_id_tag_line = riot_id["tag_line"].toString();
	v.player = v.riot_id_game_name + "#" + v.riot_id_tag_line;
	v.observed_started_at = QDateTime::fromString(capture["started_at_utc"].toString(), Qt::ISODateWithMs);
	v.completed_at = v.observed_started_at.addMSecs(capture["duration_ms"].toInt());
	v.duration_seconds = capture["duration_ms"].toInt() / 1000;
	v.game_mode = capture["game_mode"].toString();
	v.map_number = capture["map_number"].toInt();
	v.frontmost_capture = capture["frontmost_capture"].toBool();
	v.complete = capture["complete"].toBool();
	v.event_detail_truncated = capture["event_detail_truncated"].toBool();
	for (const auto value : o["input"].toObject()["intensity_by_second"].toArray()) {
		const QJsonObject sample = value.toObject();
		v.v2_intensity.append(
			{sample["second"].toInt(), sample["apm"].toDouble(), sample["mouse_velocity"].toDouble()});
	}
	const QJsonObject summary = o["input"].toObject()["summary"].toObject();
	v.v2_summary = {o["input"].toObject()["left_clicks"].toInt(),
			o["input"].toObject()["right_clicks"].toInt(),
			o["input"].toObject()["gameplay_key_actions"].toInt(),
			summary["peak_apm"].toDouble(),
			summary["median_apm"].toDouble(),
			summary["peak_mouse_velocity"].toDouble(),
			summary["median_mouse_velocity"].toDouble()};
	return true;
}

QString classify_event(const QString &name)
{
	if (name == "ChampionKill")
		return "kill";
	if (name.contains("Turret", Qt::CaseInsensitive))
		return "tower";
	if (name.contains("Dragon", Qt::CaseInsensitive) || name.contains("Baron", Qt::CaseInsensitive) ||
	    name.contains("Herald", Qt::CaseInsensitive) || name.contains("RiftScuttler", Qt::CaseInsensitive))
		return "objective";
	if (name == "LevelUp")
		return "level";
	return name == "GameEnd" ? "game_end" : "other";
}

QVector<chapter> make_chapters(const QVector<stat_sample> &samples, const QVector<event> &events)
{
	QVector<chapter> result;
	if (samples.isEmpty())
		return result;
	QVector<int> boundaries{samples.first().seconds};
	for (const auto &e : events)
		if (e.seconds - boundaries.last() > 90)
			boundaries.append(e.seconds);
	boundaries.append(samples.last().seconds + 1);
	for (int n = 0; n + 1 < boundaries.size(); ++n) {
		const int start = boundaries[n], end = boundaries[n + 1] - 1;
		auto first = std::find_if(samples.cbegin(), samples.cend(),
					  [&](const auto &s) { return s.seconds >= start; });
		auto last = std::find_if(samples.crbegin(), samples.crend(),
					 [&](const auto &s) { return s.seconds <= end; });
		if (first == samples.cend() || last == samples.crend())
			continue;
		QStringList changes;
		if (last->kills != first->kills || last->deaths != first->deaths || last->assists != first->assists)
			changes << QString("K/D/A %1/%2/%3 → %4/%5/%6")
					   .arg(first->kills)
					   .arg(first->deaths)
					   .arg(first->assists)
					   .arg(last->kills)
					   .arg(last->deaths)
					   .arg(last->assists);
		if (last->cs != first->cs)
			changes << QString("CS %1 → %2").arg(first->cs).arg(last->cs);
		if (last->level != first->level)
			changes << QString("level %1 → %2").arg(first->level).arg(last->level);
		result.append({start, end, changes.isEmpty() ? "Observed activity window." : changes.join(", ")});
	}
	return result;
}

QVector<insight> make_insights(const report &v)
{
	QVector<insight> result;
	if (!v.item_events.isEmpty())
		result.append({"First completed item",
			       QString("%1 at %2:%3")
				       .arg(v.item_events.first().item)
				       .arg(v.item_events.first().seconds / 60)
				       .arg(v.item_events.first().seconds % 60, 2, 10, QLatin1Char('0'))});
	for (const auto &a : v.abilities)
		if (a.level == 6 || a.level == 11 || a.level == 16)
			result.append({"Level milestone", QString("Level %1 at %2:%3")
								  .arg(a.level)
								  .arg(a.seconds / 60)
								  .arg(a.seconds % 60, 2, 10, QLatin1Char('0'))});
	return result;
}

QVector<double> normalized_series(const QVector<double> &values, bool average_ratio)
{
	QVector<double> result;
	if (values.isEmpty())
		return result;
	const double average = std::accumulate(values.begin(), values.end(), 0.0) / values.size();
	const auto [low, high] = std::minmax_element(values.begin(), values.end());
	const double denominator = average_ratio ? average : *high - *low;
	for (double value : values)
		result.append(denominator == 0 ? 0.0
					       : (average_ratio ? value / denominator : (value - *low) / denominator));
	return result;
}
QString display_name(const report &v)
{
	return QString("%1 — %2").arg(v.completed_at.toLocalTime().toString("yyyy-MM-dd HH:mm"),
				      v.game_mode.isEmpty() ? "League game" : v.game_mode);
}
} // namespace sources::lol_game_report
