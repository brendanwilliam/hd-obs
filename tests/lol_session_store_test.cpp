#include "sources/game_report/data/lol_session_store.hpp"

#include <QTemporaryDir>
#include <QTimeZone>

#include <algorithm>
#include <cassert>

int main()
{
	using namespace sources::lol_game_report;
	QTemporaryDir directory;
	assert(directory.isValid());
	session_store store(directory.path());
	for (int index = 0; index < 21; ++index) {
		retained_session session;
		session.value.id = QString("00000000-0000-4000-8000-%1").arg(index, 12, 10, QLatin1Char('0'));
		session.value.riot_id_game_name = "Player";
		session.value.riot_id_tag_line = "NA1";
		session.value.observed_started_at = QDateTime::fromMSecsSinceEpoch(index * 1000, QTimeZone::UTC);
		session.value.completed_at = session.value.observed_started_at.addSecs(60);
		session.value.duration_seconds = 60;
		session.value.complete = true;
		session.value.local_gameplay_events =
			QJsonArray{QJsonObject{{"kind", "bound_key"}, {"action", "spell_1"}}};
		assert(store.save(session));
	}
	const auto sessions = store.load();
	assert(sessions.size() == 20);
	assert(!std::any_of(sessions.cbegin(), sessions.cend(),
			    [](const retained_session &session) { return session.value.id.endsWith("000000000000"); }));
	assert(store.update_upload(sessions.first().value.id, upload_state::confirmed, {}, 1));
	const auto updated = store.load();
	assert(std::any_of(updated.cbegin(), updated.cend(),
			   [](const retained_session &session) { return session.upload == upload_state::confirmed; }));
	return 0;
}
