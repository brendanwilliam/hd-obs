#include "sources/game_report/presentation/lol_report_manager.hpp"

#include "sources/game_report/collection/lol_collector.hpp"
#include "sources/dashboard/detection/lol_input_bindings.hpp"
#include "sources/game_report/integration/lol_online_reports.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QPointer>
#include <obs-module.h>

namespace sources {
namespace {
constexpr const char *development_logs_key = "lol_dashboard.report.development_logs";
constexpr const char *online_service_url_key = "lol_dashboard.report.online_service_url";
constexpr const char *upload_enabled_key = "lol_dashboard.report.upload_enabled";
constexpr const char *analysis_enabled_key = "lol_dashboard.report.analysis_enabled";
constexpr const char *game_config_key = "lol_dashboard.game_cfg";
} // namespace

class lol_report_manager::implementation {
public:
	implementation()
	{
		const QPointer<lol_game_report::online_reports> online_reports = &online;
		collector.set_submission_callback([online_reports](const lol_game_report::report &report) {
			if (!online_reports)
				return;
			QMetaObject::invokeMethod(
				online_reports,
				[online_reports, report] {
					if (online_reports)
						online_reports->submit(report);
				},
				Qt::QueuedConnection);
		});
	}
	~implementation()
	{
		collector.set_submission_callback({});
		if (owner == this)
			owner = nullptr;
	}
	void update(obs_data_t *settings)
	{
		development_logs = obs_data_get_bool(settings, development_logs_key);
		analysis_enabled = obs_data_get_bool(settings, analysis_enabled_key);
		collector.set_enabled(analysis_enabled);
		online.set_service_url(QString::fromUtf8(obs_data_get_string(settings, online_service_url_key)));
		online.set_upload_enabled(obs_data_get_bool(settings, upload_enabled_key));
		const QFileInfo game_config(QString::fromUtf8(obs_data_get_string(settings, game_config_key)));
		const QString next_input_path = game_config.dir().filePath("input.ini");
		if (input_path != next_input_path)
			input_stamp_ = {-1, -1};
		input_path = next_input_path;
		reload_bindings(collector.active_champion());
	}
	void tick(const QRect &game_frame)
	{
		if (owner && owner != this)
			return;
		owner = this;
		collector.set_game_frame(game_frame);
		collector.set_development_logs(development_logs);
		collector.set_enabled(analysis_enabled);
		collector.tick();
		reload_bindings(collector.active_champion());
	}
	void reload_bindings(const QString &champion)
	{
		const QFileInfo input(input_path);
		const std::pair<qint64, qint64> stamp{input.lastModified().toMSecsSinceEpoch(), input.size()};
		if (input_path.isEmpty() || (stamp == input_stamp_ && champion == input_champion_))
			return;
		QFile file(input_path);
		if (!file.open(QIODevice::ReadOnly))
			return;
		lol_input_bindings bindings;
		if (!bindings.parse(QString::fromUtf8(file.readAll()), champion))
			return;
		collector.set_gameplay_actions(bindings.gameplay_actions());
		input_stamp_ = stamp;
		input_champion_ = champion;
	}
	lol_game_report::collector collector;
	lol_game_report::online_reports online;
	bool development_logs{};
	bool analysis_enabled{};
	QString input_path;
	QString input_champion_;
	std::pair<qint64, qint64> input_stamp_{};
	static implementation *owner;
};

lol_report_manager::implementation *lol_report_manager::implementation::owner{};

lol_report_manager::lol_report_manager() : implementation_(new implementation) {}
lol_report_manager::~lol_report_manager()
{
	delete implementation_;
}
void lol_report_manager::update(obs_data *settings)
{
	implementation_->update(reinterpret_cast<obs_data_t *>(settings));
}
void lol_report_manager::tick(const QRect &game_frame)
{
	implementation_->tick(game_frame);
}
bool lol_report_manager::link_online_reports()
{
	auto &online = implementation_->online;
	QMetaObject::invokeMethod(&online, [&online] { online.begin_link(); }, Qt::QueuedConnection);
	return true;
}
bool lol_report_manager::unlink_online_reports()
{
	auto &online = implementation_->online;
	QMetaObject::invokeMethod(&online, [&online] { online.unlink(); }, Qt::QueuedConnection);
	return true;
}
bool lol_report_manager::retry_online_reports()
{
	auto &online = implementation_->online;
	QMetaObject::invokeMethod(&online, [&online] { online.retry(); }, Qt::QueuedConnection);
	return true;
}
void lol_report_manager::defaults(obs_data *settings)
{
	auto *value = reinterpret_cast<obs_data_t *>(settings);
	obs_data_set_default_bool(value, development_logs_key, false);
	obs_data_set_default_bool(value, analysis_enabled_key, false);
	obs_data_set_default_bool(value, upload_enabled_key, true);
	obs_data_set_default_string(value, online_service_url_key, ONLINE_REPORTS_SERVICE_URL);
}
void lol_report_manager::add_properties(obs_properties *properties)
{
	auto *props = reinterpret_cast<obs_properties_t *>(properties);
	auto *online = obs_properties_create();
	obs_properties_add_bool(online, upload_enabled_key, obs_module_text("LoLGameReport.UploadEnabled"));
	obs_properties_add_text(online, online_service_url_key, obs_module_text("LoLGameReport.OnlineServiceURL"),
				OBS_TEXT_DEFAULT);
	const QString status =
		QString("%1: %2").arg(obs_module_text("LoLGameReport.CollectorStatus"),
				      lol_game_report::collector::state_text(implementation_->collector.state()));
	obs_properties_add_text(online, "lol_dashboard.report.online_status",
				QString("%1: %2").arg(status, implementation_->online.status()).toUtf8().constData(),
				OBS_TEXT_INFO);
	obs_properties_add_bool(online, development_logs_key, obs_module_text("LoLGameReport.DevelopmentLogs"));
	obs_properties_add_button2(
		online, "lol_dashboard.report.online_link", obs_module_text("LoLGameReport.OnlineLink"),
		[](obs_properties_t *, obs_property_t *, void *data) {
			return static_cast<lol_report_manager *>(data)->link_online_reports();
		},
		this);
	obs_properties_add_button2(
		online, "lol_dashboard.report.online_unlink", obs_module_text("LoLGameReport.OnlineUnlink"),
		[](obs_properties_t *, obs_property_t *, void *data) {
			return static_cast<lol_report_manager *>(data)->unlink_online_reports();
		},
		this);
	obs_properties_add_button2(
		online, "lol_dashboard.report.online_retry", obs_module_text("LoLGameReport.OnlineRetry"),
		[](obs_properties_t *, obs_property_t *, void *data) {
			return static_cast<lol_report_manager *>(data)->retry_online_reports();
		},
		this);
	auto *online_group = obs_properties_add_group(props, "lol_dashboard.report.online",
						      obs_module_text("LoLGameReport.Online"), OBS_GROUP_NORMAL,
						      online);
	Q_UNUSED(online_group);
}
} // namespace sources
