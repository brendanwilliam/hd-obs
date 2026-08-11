struct shared_collector {
	std::mutex mutex;
	std::mutex champion_mutex;
	int references{};
	int development_log_references{};
	std::atomic<collection_state> state{collection_state::empty};
	QThread thread;
	worker *worker{};
	QString active_champion;
	void acquire()
	{
		std::lock_guard lock(mutex);
		if (references++ != 0)
			return;
		worker = new class worker(state);
		worker->set_champion_callback([this](const QString &champion) {
			std::lock_guard lock(champion_mutex);
			active_champion = champion;
		});
		worker->moveToThread(&thread);
		QObject::connect(&thread, &QThread::started, worker, [this] { worker->start(); });
		thread.start();
	}
	void release()
	{
		std::lock_guard lock(mutex);
		if (references <= 0)
			return;
		if (--references != 0)
			return;
		QMetaObject::invokeMethod(worker, [this] { worker->stop(); }, Qt::BlockingQueuedConnection);
		thread.quit();
		thread.wait();
		delete worker;
		worker = nullptr;
		{
			std::lock_guard champion_lock(champion_mutex);
			active_champion.clear();
		}
		state = collection_state::empty;
	}
	void set_game_frame(const QRect &frame)
	{
		QMetaObject::invokeMethod(
			worker, [this, frame] { worker->set_game_frame(frame); }, Qt::QueuedConnection);
	}
	void set_gameplay_actions(const QHash<QString, QString> &actions)
	{
		QMetaObject::invokeMethod(
			worker, [this, actions] { worker->set_gameplay_actions(actions); }, Qt::QueuedConnection);
	}
	void set_enabled(bool enabled)
	{
		QMetaObject::invokeMethod(
			worker, [this, enabled] { worker->set_enabled(enabled); }, Qt::QueuedConnection);
	}
	QString champion()
	{
		std::lock_guard lock(champion_mutex);
		return active_champion;
	}
	void consume_input(const std::vector<input_data::trace_event> &events)
	{
		if (!events.empty())
			QMetaObject::invokeMethod(
				worker, [this, events] { worker->consume_input(events); }, Qt::QueuedConnection);
	}
	void set_submission_callback(std::function<void(const report &)> callback)
	{
		QMetaObject::invokeMethod(
			worker, [callback = std::move(callback), this] { worker->set_submission_callback(callback); },
			Qt::QueuedConnection);
	}
	void set_development_logs(bool enabled)
	{
		std::lock_guard lock(mutex);
		if (enabled)
			++development_log_references;
		else if (development_log_references > 0)
			--development_log_references;
		if (!worker)
			return;
		const bool active = development_log_references > 0;
		QMetaObject::invokeMethod(
			worker, [this, active] { worker->set_development_logs(active); }, Qt::QueuedConnection);
	}
	bool development_logs_enabled()
	{
		std::lock_guard lock(mutex);
		return development_log_references > 0;
	}
	QString development_log_path()
	{
		QString result;
		std::lock_guard lock(mutex);
		if (worker)
			QMetaObject::invokeMethod(
				worker, [this, &result] { result = worker->development_log_path(); },
				Qt::BlockingQueuedConnection);
		return result;
	}
};
shared_collector &shared()
{
	static shared_collector value;
	return value;
}
} // namespace

collector::collector()
{
	shared().acquire();
}
collector::~collector()
{
	if (development_logs_)
		shared().set_development_logs(false);
	shared().release();
}
collection_state collector::state() const
{
	return shared().state.load();
}
void collector::set_game_frame(const QRect &frame)
{
	shared().set_game_frame(frame);
}
void collector::set_gameplay_actions(const QHash<QString, QString> &actions)
{
	shared().set_gameplay_actions(actions);
}
void collector::set_enabled(bool enabled)
{
	shared().set_enabled(enabled);
}
QString collector::active_champion() const
{
	return shared().champion();
}
void collector::tick()
{
	if (!uiohook::league_game_is_frontmost()) {
		discard_backlog_ = true;
		return;
	}
	std::vector<input_data::trace_event> events;
	input_data::button_map<uint16_t> keyboard, mouse;
	input_broker::consume({}, cursor_, discard_backlog_, events, keyboard, mouse);
	// The broker carries key characters for live-key displays. Reports retain only action counts.
	for (auto &event : events)
		event.keychar = 0;
	shared().consume_input(events);
}
void collector::set_submission_callback(std::function<void(const report &)> callback)
{
	shared().set_submission_callback(std::move(callback));
}
void collector::set_development_logs(bool enabled)
{
	if (development_logs_ == enabled)
		return;
	development_logs_ = enabled;
	shared().set_development_logs(enabled);
}
bool collector::development_logs_enabled() const
{
	return shared().development_logs_enabled();
}
QString collector::development_log_path() const
{
	return shared().development_log_path();
}
QString collector::state_text(collection_state value)
{
	if (value == collection_state::recording)
		return "Recording self-only local game data";
	if (value == collection_state::finalizing)
		return "Finalizing report";
	return "Waiting for a local League game";
}
