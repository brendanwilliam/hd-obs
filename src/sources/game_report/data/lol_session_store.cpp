#include "sources/game_report/data/lol_session_store.hpp"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QSaveFile>

#include <algorithm>

namespace sources::lol_game_report {
namespace {
constexpr int maximum_sessions = 20;

QString state_name(upload_state state)
{
	if (state == upload_state::confirmed)
		return "confirmed";
	if (state == upload_state::rejected)
		return "rejected";
	return "pending";
}

upload_state state_from_name(const QString &value)
{
	if (value == "confirmed")
		return upload_state::confirmed;
	if (value == "rejected")
		return upload_state::rejected;
	return upload_state::pending;
}

QJsonObject session_json(const retained_session &session)
{
	const QJsonObject payload = to_json(session.value);
	QJsonObject object{{"schema_version", payload["schema_version"].toInt()},
			   {"payload", payload},
			   {"upload_state", state_name(session.upload)},
			   {"retry_at",
			    session.retry_at.toUTC().toString(Qt::ISODateWithMs)},
			   {"attempts", session.attempts}};
	if (payload["schema_version"].toInt() == 2)
		object.insert("local_gameplay_events",
			      session.value.local_gameplay_events);
	return object;
}

bool session_from_json(const QJsonObject &object, retained_session &session)
{
	const int schema_version = object["schema_version"].toInt();
	if ((schema_version != 2 && schema_version != 3) ||
	    !from_json(object["payload"].toObject(), session.value))
		return false;
	// Legacy checkpoints can contain collector-only chords, sequence IDs, and monotonic
	// timestamps. Do not retain them when resuming a report.
	session.value.local_gameplay_events = {};
	session.upload = state_from_name(object["upload_state"].toString());
	session.retry_at =
		QDateTime::fromString(object["retry_at"].toString(), Qt::ISODateWithMs);
	session.attempts = object["attempts"].toInt();
	return true;
}
} // namespace

session_store::session_store(QString root) : root_(std::move(root))
{
	QDir().mkpath(root_);
}

QString session_store::path_for(const QString &report_id) const
{
	return root_ + "/" + report_id + ".json";
}

QString session_store::checkpoint_path() const
{
	return root_ + "/in-progress.json";
}

bool session_store::save(const retained_session &session)
{
	if (session.value.id.isEmpty())
		return false;
	QSaveFile file(path_for(session.value.id));
	if (!file.open(QIODevice::WriteOnly))
		return false;
	if (file.write(
		    QJsonDocument(session_json(session)).toJson(QJsonDocument::Compact)) <
		    0 ||
	    !file.commit())
		return false;
	auto sessions = load();
	std::sort(sessions.begin(), sessions.end(),
		  [](const retained_session &left, const retained_session &right) {
			  return left.value.completed_at < right.value.completed_at;
		  });
	while (sessions.size() > maximum_sessions)
		QFile::remove(path_for(sessions.takeFirst().value.id));
	return true;
}

QVector<retained_session> session_store::load() const
{
	QVector<retained_session> sessions;
	for (const QFileInfo &entry :
	     QDir(root_).entryInfoList({"*.json"}, QDir::Files)) {
		QFile file(entry.filePath());
		if (!file.open(QIODevice::ReadOnly))
			continue;
		retained_session session;
		if (session_from_json(QJsonDocument::fromJson(file.readAll()).object(),
				      session))
			sessions.append(std::move(session));
	}
	return sessions;
}

bool session_store::update_upload(const QString &report_id, upload_state state,
				  QDateTime retry_at, int attempts)
{
	auto sessions = load();
	for (auto &session : sessions) {
		if (session.value.id != report_id)
			continue;
		session.upload = state;
		session.retry_at = retry_at;
		session.attempts = attempts;
		return save(session);
	}
	return false;
}

bool session_store::save_checkpoint(const retained_session &session)
{
	if (session.value.id.isEmpty())
		return false;
	QSaveFile file(checkpoint_path());
	if (!file.open(QIODevice::WriteOnly))
		return false;
	return file.write(QJsonDocument(session_json(session))
				  .toJson(QJsonDocument::Compact)) >= 0 &&
	       file.commit();
}

std::optional<retained_session> session_store::load_checkpoint() const
{
	QFile file(checkpoint_path());
	if (!file.open(QIODevice::ReadOnly))
		return std::nullopt;
	retained_session session;
	return session_from_json(QJsonDocument::fromJson(file.readAll()).object(),
				 session)
		       ? std::optional{session}
		       : std::nullopt;
}

bool session_store::clear_checkpoint()
{
	return !QFile::exists(checkpoint_path()) || QFile::remove(checkpoint_path());
}

} // namespace sources::lol_game_report
