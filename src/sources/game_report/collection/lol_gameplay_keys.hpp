#pragma once

#include <QSet>
#include <QString>

#include <cstdint>

namespace sources::lol_game_report {

QString gameplay_key_name(uint16_t code);
QString gameplay_modifier_name(uint16_t code);
QString gameplay_chord(QSet<QString> modifiers, uint16_t trigger);

} // namespace sources::lol_game_report
