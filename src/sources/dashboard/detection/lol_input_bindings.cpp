#include "sources/dashboard/detection/lol_input_bindings.hpp"

#include <QRegularExpression>

#include <algorithm>

namespace sources {
namespace {
QString action_for(const QString &event)
{
	static const QHash<QString, QString> direct{{"evtCastSpell1", "spell_1"},
						    {"evtCastSpell2", "spell_2"},
						    {"evtCastSpell3", "spell_3"},
						    {"evtCastSpell4", "spell_4"},
						    {"evtCastAvatarSpell1", "summoner_1"},
						    {"evtCastAvatarSpell2", "summoner_2"},
						    {"evtUseVisionItem", "trinket"},
						    {"evtCastRoleBound", "role_bound"},
						    {"evtUseItem7", "recall"},
						    {"evtOpenShop", "shop"},
						    {"evtPlayerAttackMove", "attack_move"},
						    {"evtPlayerAttackMoveClick", "attack_move_click"},
						    {"evtPlayerAttackOnlyClick", "attack_only_click"},
						    {"evtPlayerHoldPosition", "stop"},
						    {"evtPlayerStopPosition", "stop"}};
	if (direct.contains(event))
		return direct.value(event);
	const QRegularExpression casts(
		"^evt(SelfCast|NormalCast|SmartCast|SmartPlusSelfCast|SmartCastWithIndicator|"
		"SmartPlusSelfCastWithIndicator)(Spell|AvatarSpell|Item[1-6]|VisionItem|RoleBound)([1-4])?$");
	const auto cast = casts.match(event);
	if (cast.hasMatch()) {
		const QString kind = cast.captured(2);
		QString action;
		if (kind == "Spell")
			action = "spell_" + cast.captured(3);
		else if (kind == "AvatarSpell")
			action = "summoner_" + cast.captured(3);
		else if (kind.startsWith("Item"))
			action = "item_" + kind.right(1);
		else if (kind == "VisionItem")
			action = "trinket";
		else
			action = "role_bound";
		const QString prefix =
			cast.captured(1).replace(QRegularExpression("([a-z])([A-Z])"), "\\1_\\2").toLower();
		return prefix + "_" + action;
	}
	const QRegularExpression items("^evtUseItem([1-6])$");
	const auto match = items.match(event);
	return match.hasMatch() ? "item_" + match.captured(1) : QString{};
}
QString normalize(QString value)
{
	value = value.trimmed();
	if (value.compare("Control", Qt::CaseInsensitive) == 0)
		return "Ctrl";
	if (value.compare("Command", Qt::CaseInsensitive) == 0)
		return "Cmd";
	return value.left(1).toUpper() + value.mid(1).toLower();
}

QString canonical_chord(QStringList parts)
{
	if (parts.isEmpty())
		return {};
	const QString trigger = parts.takeLast();
	std::sort(parts.begin(), parts.end());
	parts.append(trigger);
	return parts.join('+');
}
} // namespace

bool lol_input_bindings::parse(const QString &contents, const QString &champion)
{
	QHash<QString, QString> base, override;
	QString section;
	for (const QString &line : contents.split('\n')) {
		const QString trimmed = line.trimmed();
		if (trimmed.startsWith('[') && trimmed.endsWith(']')) {
			section = trimmed.mid(1, trimmed.size() - 2);
			continue;
		}
		const qsizetype separator = trimmed.indexOf('=');
		if (separator <= 0)
			continue;
		const QString key = trimmed.left(separator).trimmed();
		const QString value = trimmed.mid(separator + 1).trimmed();
		if (section == "GameEvents")
			base.insert(key, value);
		if (!champion.isEmpty() && section.compare("GameEvents." + champion, Qt::CaseInsensitive) == 0)
			override.insert(key, value);
	}
	for (auto item = override.cbegin(); item != override.cend(); ++item)
		base.insert(item.key(), item.value());
	by_chord_.clear();
	const QRegularExpression tokens("\\[([^\\]]+)\\]");
	for (auto item = base.cbegin(); item != base.cend(); ++item) {
		const QString action = action_for(item.key());
		if (action.isEmpty())
			continue;
		for (const QString &entry : item.value().split(',')) {
			QStringList parts;
			auto match = tokens.globalMatch(entry);
			while (match.hasNext())
				parts.append(normalize(match.next().captured(1)));
			if (parts.isEmpty() || parts.contains("<Unbound>", Qt::CaseInsensitive))
				continue;
			const QString trigger = parts.takeLast();
			lol_binding binding{action, canonical_chord(parts + QStringList{trigger}), trigger, parts,
					    false};
			const QString key = binding.chord;
			if (by_chord_.contains(key))
				by_chord_[key].ambiguous = true;
			else
				by_chord_.insert(key, binding);
		}
	}
	return !by_chord_.isEmpty();
}

const lol_binding *lol_input_bindings::resolve(const QString &trigger, const QStringList &modifiers) const
{
	QStringList parts;
	for (const QString &modifier : modifiers)
		parts.append(normalize(modifier));
	parts.append(normalize(trigger));
	const auto found = by_chord_.constFind(canonical_chord(parts));
	return found == by_chord_.cend() || found->ambiguous ? nullptr : &found.value();
}
QHash<QString, QString> lol_input_bindings::gameplay_actions() const
{
	QHash<QString, QString> result;
	for (auto binding = by_chord_.cbegin(); binding != by_chord_.cend(); ++binding)
		if (!binding->ambiguous)
			result.insert(binding.key(), binding->action);
	return result;
}
qsizetype lol_input_bindings::size() const
{
	return by_chord_.size();
}
} // namespace sources
