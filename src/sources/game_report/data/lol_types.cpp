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
	QString game_name = v.player, tag_line = "unknown";
	const qsizetype separator = game_name.lastIndexOf('#');
	if (separator > 0) {
		tag_line = game_name.mid(separator + 1);
		game_name.truncate(separator);
	}
	QJsonArray intensity;
	double peak_apm{}, peak_velocity{};
	QVector<double> apm, velocity;
	for (const auto &sample : v.input_samples) {
		const double value = sample.actions * 20.0;
		const double movement = sample.max_velocity_pixels_per_second;
		intensity.append(QJsonObject{{"second", sample.seconds}, {"apm", value}, {"mouse_velocity", movement}});
		apm.append(value);
		velocity.append(movement);
		peak_apm = std::max(peak_apm, value);
		peak_velocity = std::max(peak_velocity, movement);
	}
	auto median = [](QVector<double> values) {
		if (values.isEmpty())
			return 0.0;
		std::sort(values.begin(), values.end());
		const qsizetype middle = values.size() / 2;
		return values.size() % 2 ? values[middle] : (values[middle - 1] + values[middle]) / 2.0;
	};
	return {{"schema_version", 2},
		{"report_id", v.id},
		{"capture_policy_version", 1},
		{"capture",
		 QJsonObject{{"started_at_utc",
			      v.completed_at.addSecs(-v.duration_seconds).toUTC().toString(Qt::ISODateWithMs)},
			     {"duration_ms", v.duration_seconds * 1000},
			     {"game_mode", "CLASSIC"},
			     {"map_number", 11},
			     {"riot_id", QJsonObject{{"game_name", game_name}, {"tag_line", tag_line}}},
			     {"frontmost_capture", true},
			     {"complete", v.duration_seconds > 0},
			     {"event_detail_truncated", false}}},
		{"input", QJsonObject{{"left_clicks", 0},
				      {"right_clicks", 0},
				      {"gameplay_key_actions", 0},
				      {"intensity_by_second", intensity},
				      {"summary", QJsonObject{{"peak_apm", peak_apm},
							      {"median_apm", median(apm)},
							      {"peak_mouse_velocity", peak_velocity},
							      {"median_mouse_velocity", median(velocity)}}}}},
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
	v.player = riot_id["game_name"].toString() + "#" + riot_id["tag_line"].toString();
	v.completed_at = QDateTime::fromString(capture["started_at_utc"].toString(), Qt::ISODateWithMs)
				 .addMSecs(capture["duration_ms"].toInt());
	v.duration_seconds = capture["duration_ms"].toInt() / 1000;
	v.game_mode = capture["game_mode"].toString();
	for (const auto value : o["input"].toObject()["intensity_by_second"].toArray()) {
		const QJsonObject sample = value.toObject();
		v.input_samples.append({sample["second"].toInt(), int(sample["apm"].toDouble() / 20.0), 0,
					sample["mouse_velocity"].toDouble()});
	}
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
