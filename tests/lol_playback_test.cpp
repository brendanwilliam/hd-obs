#include "sources/game_report/collection/lol_playback.hpp"

#include <QJsonDocument>

#include <cassert>

void run_playback_tests()
{
	using namespace sources::lol_game_report;
	playback_stream stream;
	assert(append_playback_record(stream, {100, playback_kind::pointer_sample,
					       QPointF{0.25, 0.75}}));
	assert(!append_playback_record(stream, {150, playback_kind::pointer_sample,
						QPointF{0.5, 0.5}}));
	assert(append_playback_record(stream, {200, playback_kind::left_click,
					       QPointF{0.25, 0.75}}));
	assert(append_playback_record(stream, {300, playback_kind::gameplay_action,
					       std::nullopt, "spell_1"}));
	assert(!append_playback_record(
		stream, {400, playback_kind::gameplay_action, std::nullopt, {}}));
	assert(!append_playback_record(stream, {500, playback_kind::pointer_sample,
						QPointF{1.1, 0.5}}));
	assert(!append_playback_record(stream,
				       {600, playback_kind::left_click, std::nullopt}));
	assert(!append_playback_record(stream, {700, playback_kind::gameplay_action,
						std::nullopt, "Q"}));
	const QJsonArray records = playback_json(stream);
	assert(records.size() == 3);
	assert(records[1].toObject()["kind"] == "left_click");
	assert(!records[2].toObject().contains("pointer"));
	assert(!records[2].toObject().contains("sequence"));
	assert(playback_canonical_array_bytes(stream) ==
	       QJsonDocument(records).toJson(QJsonDocument::Compact).size());
}
