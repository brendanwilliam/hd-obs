#include "sources/game_report/collection/lol_collector.hpp"

#include "sources/game_report/collection/lol_game_context.hpp"
#include "sources/game_report/collection/lol_gameplay_keys.hpp"
#include "sources/game_report/data/lol_diagnostics.hpp"

#include "hook/uiohook_helper.hpp"
#include "input/input_broker.hpp"

#include <QDateTime>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPluginLoader>
#include <QSslConfiguration>
#include <QSslSocket>
#include <QThread>
#include <QTimer>
#include <QUuid>

#include <atomic>
#include <algorithm>
#include <cmath>
#include <memory>
#include <mutex>
#include <obs-module.h>

extern "C" {
#include <util/bmem.h>
#include <util/platform.h>
}

namespace sources::lol_game_report {
namespace {
constexpr uint64_t second_ns = 1000000000ULL;

class worker final : public QObject {
public:
	explicit worker(std::atomic<collection_state> &state) : state_(state) {}

	void start()
	{
		load_tls_backend();
		manager_ = new QNetworkAccessManager(this);
		timer_ = new QTimer(this);
		QObject::connect(timer_, &QTimer::timeout, this, [this] { poll(); });
		timer_->start(1000);
		poll();
	}
	void stop()
	{
		if (timer_)
			timer_->stop();
		diagnostics_.write("collector", "worker_stopped");
		diagnostics_.close_and_remove();
	}
	void set_submission_callback(std::function<void(const report &)> callback)
	{
		submission_callback_ = std::move(callback);
	}
	void set_dpi(int) {}
	void set_hex_radius_percent(double) {}
	void set_development_logs(bool enabled)
	{
		diagnostics_.set_enabled(enabled);
		diagnostics_.write("collector", "development_logs_changed", {{"enabled", enabled}});
	}
	bool development_logs_enabled() const { return diagnostics_.enabled(); }
	QString development_log_path() const { return diagnostics_.path(); }
	void set_game_frame(const QRect &frame)
	{
		if (!active_)
			game_frame_ = frame;
	}
	void set_champion_callback(std::function<void(const QString &)> callback)
	{
		champion_callback_ = std::move(callback);
	}
	void set_gameplay_actions(const QHash<QString, QString> &actions) { gameplay_actions_ = actions; }
	void set_enabled(bool enabled)
	{
		enabled_ = enabled;
		if (!enabled_ && active_) {
			active_ = false;
			invalid_polls_ = 0;
			metrics_.reset();
			state_ = collection_state::empty;
			diagnostics_.write("collector", "report_discarded", {{"reason", "analysis_disabled"}});
		}
	}
	void consume_input(const std::vector<input_data::trace_event> &events)
	{
		if (!active_)
			return;
		for (const auto &event : events)
			consume_event(event);
	}

private:
	void load_tls_backend()
	{
		char *path = obs_module_file("tls/libqsecuretransportbackend.dylib");
		if (!path)
			return;
		tls_backend_ = std::make_unique<QPluginLoader>(QString::fromUtf8(path));
		bfree(path);
		tls_backend_->instance();
	}
	void poll()
	{
		if (pending_)
			return;
		pending_ = true;
		QNetworkRequest request(QUrl("https://127.0.0.1:2999/liveclientdata/allgamedata"));
		request.setTransferTimeout(1200);
		QSslConfiguration ssl = request.sslConfiguration();
		ssl.setPeerVerifyMode(QSslSocket::VerifyNone);
		request.setSslConfiguration(ssl);
		auto *reply = manager_->get(request);
		reply->ignoreSslErrors();
		QObject::connect(reply, &QNetworkReply::finished, reply, [this, reply] {
			pending_ = false;
			const bool success = reply->error() == QNetworkReply::NoError;
			const QJsonObject data = success ? QJsonDocument::fromJson(reply->readAll()).object()
							 : QJsonObject{};
			diagnostics_.write(
				"collector", "endpoint_completed",
				{{"endpoint", "allgamedata"},
				 {"success", success},
				 {"http_status", reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()}});
			process(success ? parse_game_context(data) : game_context{});
			reply->deleteLater();
		});
	}
	void process(const game_context &context)
	{
		if (!enabled_)
			return;
		if (!supported_game(context)) {
			if (active_ && (context.game_end || ++invalid_polls_ >= 3))
				finalize(context.game_end ? "game_end" : "invalid_game_state");
			return;
		}
		if (context.champion != active_champion_) {
			active_champion_ = context.champion;
			if (champion_callback_)
				champion_callback_(active_champion_);
		}
		invalid_polls_ = 0;
		if (!active_) {
			if (context.game_time > 1.0)
				return;
			begin(context);
		}
		last_game_seconds_ = context.game_time;
		anchor_monotonic_ns_ = os_gettime_ns();
		metrics_.evaluate_through(int(std::floor(context.game_time)));
		if (context.game_end)
			finalize("game_end");
	}
	void begin(const game_context &context)
	{
		report_ = {};
		report_.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
		report_.observed_started_at = QDateTime::currentDateTimeUtc();
		report_.riot_id_game_name = context.riot_id_game_name;
		report_.riot_id_tag_line = context.riot_id_tag_line;
		report_.champion = context.champion;
		report_.game_mode = context.game_mode;
		report_.map_number = context.map_number;
		report_.map = "Map" + QString::number(context.map_number);
		metrics_.reset();
		pressed_modifiers_.clear();
		last_game_seconds_ = context.game_time;
		anchor_monotonic_ns_ = os_gettime_ns();
		active_ = true;
		state_ = collection_state::recording;
		diagnostics_.write("collector", "report_started",
				   {{"map", context.map_number}, {"queue", context.queue_id}});
	}
	double event_game_seconds(uint64_t time_ns) const
	{
		if (time_ns <= anchor_monotonic_ns_)
			return last_game_seconds_;
		return last_game_seconds_ + double(time_ns - anchor_monotonic_ns_) / double(second_ns);
	}
	bool in_frame(const input_data::trace_event &event) const { return game_frame_.contains(event.x, event.y); }
	QPointF point_for(const input_data::trace_event &event) const
	{
		return {double(event.x - game_frame_.left()) / std::max(1, game_frame_.width()),
			double(event.y - game_frame_.top()) / std::max(1, game_frame_.height())};
	}
	void append_local_event(const QString &kind, const QString &button, const input_data::trace_event &event,
				double seconds)
	{
		if (!in_frame(event))
			return;
		const QPointF point = point_for(event);
		report_.local_gameplay_events.append(
			QJsonObject{{"sequence", QString::number(event.sequence)},
				    {"game_time_ms", qRound64(seconds * 1000.0)},
				    {"kind", kind},
				    {"button", button},
				    {"pointer", QJsonObject{{"x", point.x()}, {"y", point.y()}}}});
	}
	void consume_event(const input_data::trace_event &event)
	{
		const double seconds = event_game_seconds(event.time_ns);
		const QString modifier = gameplay_modifier_name(event.code);
		if (!modifier.isEmpty()) {
			if (event.type == EVENT_KEY_PRESSED)
				pressed_modifiers_.insert(modifier);
			else if (event.type == EVENT_KEY_RELEASED)
				pressed_modifiers_.remove(modifier);
			return;
		}
		if (event.type == EVENT_KEY_PRESSED) {
			const QString action = gameplay_actions_.value(gameplay_chord(pressed_modifiers_, event.code));
			if (!action.isEmpty()) {
				metrics_.record_action(seconds, gameplay_input::bound_key);
				report_.local_gameplay_events.append(
					QJsonObject{{"sequence", QString::number(event.sequence)},
						    {"game_time_ms", qRound64(seconds * 1000.0)},
						    {"kind", "bound_key"},
						    {"action", action}});
			}
			return;
		}
		if (event.type == EVENT_MOUSE_MOVED || event.type == EVENT_MOUSE_DRAGGED) {
			metrics_.record_motion(seconds, point_for(event), in_frame(event));
			return;
		}
		if (event.type != EVENT_MOUSE_PRESSED || !in_frame(event))
			return;
		if (event.code == MOUSE_BUTTON1) {
			metrics_.record_action(seconds, gameplay_input::left_click);
			append_local_event("mouse_button", "left", event, seconds);
		} else if (event.code == MOUSE_BUTTON2) {
			metrics_.record_action(seconds, gameplay_input::right_click);
			append_local_event("mouse_button", "right", event, seconds);
		} else if (event.code == MOUSE_BUTTON3) {
			metrics_.record_action(seconds, gameplay_input::middle_click);
			append_local_event("mouse_button", "middle", event, seconds);
		}
	}
	void finalize(const QString &reason)
	{
		if (!active_)
			return;
		state_ = collection_state::finalizing;
		metrics_.evaluate_through(int(std::floor(last_game_seconds_)));
		report_.duration_seconds = std::max(1, int(std::ceil(last_game_seconds_)));
		report_.completed_at = QDateTime::currentDateTimeUtc();
		report_.complete = true;
		report_.v2_intensity = metrics_.intensity();
		report_.v2_summary = metrics_.summary();
		diagnostics_.write("collector", "report_finalized",
				   {{"reason", reason},
				    {"duration_seconds", report_.duration_seconds},
				    {"event_count", report_.local_gameplay_events.size()}});
		if (submission_callback_)
			submission_callback_(report_);
		active_ = false;
		invalid_polls_ = 0;
		state_ = collection_state::empty;
	}

	std::atomic<collection_state> &state_;
	QNetworkAccessManager *manager_{};
	QTimer *timer_{};
	std::unique_ptr<QPluginLoader> tls_backend_;
	report report_;
	v2_metrics metrics_;
	QRect game_frame_{0, 0, 1920, 1080};
	QHash<QString, QString> gameplay_actions_;
	QSet<QString> pressed_modifiers_;
	std::function<void(const report &)> submission_callback_;
	std::function<void(const QString &)> champion_callback_;
	diagnostic_log diagnostics_;
	uint64_t anchor_monotonic_ns_{};
	double last_game_seconds_{};
	int invalid_polls_{};
	bool pending_{};
	bool active_{};
	bool enabled_{};
	QString active_champion_;
};

#include "sources/game_report/collection/lol_shared.inc"

} // namespace sources::lol_game_report
