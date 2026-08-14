#pragma once

#include "sources/game_report/data/lol_types.hpp"

#include <QDateTime>
#include <QString>
#include <QVector>

#include <optional>

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
	bool update_upload(const QString &report_id, upload_state state,
			   QDateTime retry_at, int attempts);
	bool save_checkpoint(const retained_session &session);
	std::optional<retained_session> load_checkpoint() const;
	bool clear_checkpoint();

private:
	QString root_;
	QString path_for(const QString &report_id) const;
	QString checkpoint_path() const;
};

} // namespace sources::lol_game_report
