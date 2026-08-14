#include "sources/game_report/presentation/lol_report_manager.hpp"

#include "sources/game_report/collection/lol_collector.hpp"
#include "sources/dashboard/detection/lol_input_bindings.hpp"
#include "sources/game_report/integration/lol_online_reports.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QPointer>
#include <QThread>

#include <algorithm>
#include <memory>

#include <obs-module.h>

namespace sources {
namespace {
constexpr const char *dpi_key = "lol_dashboard.report.mouse_dpi";
constexpr const char *development_logs_key = "lol_dashboard.report.development_logs";
constexpr const char *upload_enabled_key = "lol_dashboard.report.upload_enabled";
constexpr const char *analysis_enabled_key = "lol_dashboard.report.analysis_enabled";
constexpr const char *game_config_key = "lol_dashboard.game_cfg";
lol_report_manager *active_manager{};
} // namespace

class lol_report_manager::implementation {
public:
	implementation() : online(std::make_unique<lol_game_report::online_reports>())
	{
		const QPointer<lol_game_report::online_reports> online_reports =
			online.get();
		online_reports->moveToThread(&online_thread);
		online_thread.start();
		QMetaObject::invokeMethod(
			online_reports, [online_reports] { online_reports->start(); },
			Qt::QueuedConnection);
		collector.set_submission_callback(
			[online_reports](const lol_game_report::report &report) {
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
		dispose_online_reports();
		if (owner == this)
			owner = nullptr;
	}
	void dispose_online_reports()
	{
		auto *reports = online.release();
		if (!reports)
			return;
		auto dispose = [reports] {
			reports->shutdown();
			delete reports;
		};
		if (reports->thread() == QThread::currentThread()) {
			dispose();
		} else if (!QMetaObject::invokeMethod(reports, dispose,
						      Qt::BlockingQueuedConnection)) {
			blog(LOG_WARNING,
			     "[input-activity] unable to dispose online reports on its Qt thread");
		}
		online_thread.quit();
		online_thread.wait();
	}
	void update(obs_data_t *settings)
	{
		mouse_dpi =
			std::clamp(int(obs_data_get_int(settings, dpi_key)), 100, 32000);
		development_logs = obs_data_get_bool(settings, development_logs_key);
		analysis_enabled = obs_data_get_bool(settings, analysis_enabled_key);
		collector.set_enabled(analysis_enabled);
		const bool upload_enabled =
			obs_data_get_bool(settings, upload_enabled_key);
		const QPointer<lol_game_report::online_reports> online_reports =
			online.get();
		QMetaObject::invokeMethod(
			online_reports,
			[online_reports, upload_enabled] {
				if (online_reports)
					online_reports->set_upload_enabled(
						upload_enabled);
			},
			Qt::QueuedConnection);
		const QFileInfo game_config(QString::fromUtf8(
			obs_data_get_string(settings, game_config_key)));
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
		const std::pair<qint64, qint64> stamp{
			input.lastModified().toMSecsSinceEpoch(), input.size()};
		if (input_path.isEmpty() ||
		    (stamp == input_stamp_ && champion == input_champion_))
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
	QThread online_thread;
	std::unique_ptr<lol_game_report::online_reports> online;
	bool development_logs{};
	bool analysis_enabled{};
	int mouse_dpi{800};
	QString input_path;
	QString input_champion_;
	std::pair<qint64, qint64> input_stamp_{};
	static implementation *owner;
};

lol_report_manager::implementation *lol_report_manager::implementation::owner{};

lol_report_manager::lol_report_manager() : implementation_(new implementation)
{
	active_manager = this;
}
lol_report_manager::~lol_report_manager()
{
	if (active_manager == this)
		active_manager = nullptr;
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
int lol_report_manager::mouse_dpi() const
{
	return implementation_->mouse_dpi;
}
bool lol_report_manager::link_online_reports()
{
	const QPointer<lol_game_report::online_reports> online =
		implementation_->online.get();
	if (online)
		QMetaObject::invokeMethod(
			online,
			[online] {
				if (online)
					online->begin_link();
			},
			Qt::QueuedConnection);
	return true;
}
bool lol_report_manager::unlink_online_reports()
{
	const QPointer<lol_game_report::online_reports> online =
		implementation_->online.get();
	if (online)
		QMetaObject::invokeMethod(
			online,
			[online] {
				if (online)
					online->unlink();
			},
			Qt::QueuedConnection);
	return true;
}
bool lol_report_manager::retry_online_reports()
{
	const QPointer<lol_game_report::online_reports> online =
		implementation_->online.get();
	if (online)
		QMetaObject::invokeMethod(
			online,
			[online] {
				if (online)
					online->retry();
			},
			Qt::QueuedConnection);
	return true;
}
void lol_report_manager::defaults(obs_data *settings)
{
	auto *value = reinterpret_cast<obs_data_t *>(settings);
	obs_data_set_default_int(value, dpi_key, 800);
	obs_data_set_default_bool(value, development_logs_key, false);
	obs_data_set_default_bool(value, analysis_enabled_key, false);
	obs_data_set_default_bool(value, upload_enabled_key, true);
}
void lol_report_manager::add_properties(obs_properties *properties)
{
	auto *props = reinterpret_cast<obs_properties_t *>(properties);
	auto *online = obs_properties_create();
	obs_properties_add_bool(online, analysis_enabled_key,
				obs_module_text("LoLGameReport.AnalysisEnabled"));
	obs_properties_add_bool(online, upload_enabled_key,
				obs_module_text("LoLGameReport.UploadEnabled"));
	obs_properties_add_int(online, dpi_key, obs_module_text("LoLGameReport.MouseDPI"),
			       100, 32000, 50);
	const QString status =
		QString("%1: %2").arg(obs_module_text("LoLGameReport.CollectorStatus"),
				      lol_game_report::collector::state_text(
					      implementation_->collector.state()));
	obs_properties_add_text(online, "lol_dashboard.report.online_status",
				QString("%1: %2")
					.arg(status, implementation_->online->status())
					.toUtf8()
					.constData(),
				OBS_TEXT_INFO);
	obs_properties_add_bool(online, development_logs_key,
				obs_module_text("LoLGameReport.DevelopmentLogs"));
	obs_properties_add_button2(
		online, "lol_dashboard.report.online_link",
		obs_module_text("LoLGameReport.OnlineLink"),
		[](obs_properties_t *, obs_property_t *, void *data) {
			return static_cast<lol_report_manager *>(data)
				->link_online_reports();
		},
		this);
	obs_properties_add_button2(
		online, "lol_dashboard.report.online_unlink",
		obs_module_text("LoLGameReport.OnlineUnlink"),
		[](obs_properties_t *, obs_property_t *, void *data) {
			return static_cast<lol_report_manager *>(data)
				->unlink_online_reports();
		},
		this);
	obs_properties_add_button2(
		online, "lol_dashboard.report.online_retry",
		obs_module_text("LoLGameReport.OnlineRetry"),
		[](obs_properties_t *, obs_property_t *, void *data) {
			return static_cast<lol_report_manager *>(data)
				->retry_online_reports();
		},
		this);
	auto *online_group = obs_properties_add_group(
		props, "lol_dashboard.report.online",
		obs_module_text("LoLGameReport.Online"), OBS_GROUP_NORMAL, online);
	Q_UNUSED(online_group);
}
void lol_report_manager::add_active_properties(obs_properties *properties)
{
	if (active_manager)
		active_manager->add_properties(properties);
}
} // namespace sources
