#pragma once

#include "sources/game_report/data/lol_types.hpp"

#include <QDateTime>
#include <QString>
#include <QVector>

namespace sources::lol_game_report {

enum class upload_state { pending, confirmed, rejected };

struct retained_session {
	report value;
	upload_state upload{upload_state::pending};
	QDateTime retry_at;
	int attempts{};
};

class session_store {
public:
	explicit session_store(QString root);
	bool save(const retained_session &session);
	QVector<retained_session> load() const;
	bool update_upload(const QString &report_id, upload_state state, QDateTime retry_at, int attempts);

private:
	QString root_;
	QString path_for(const QString &report_id) const;
};

} // namespace sources::lol_game_report
