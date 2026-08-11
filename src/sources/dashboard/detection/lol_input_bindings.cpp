#include "sources/dashboard/detection/lol_input_bindings.hpp"

#include <QRegularExpression>

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
						    {"evtOpenShop", "shop"}};
	if (direct.contains(event))
		return direct.value(event);
	const QRegularExpression casts("^evt(SelfCast|NormalCast|SmartCast)(Spell|AvatarSpell)([1-4])$");
	const auto cast = casts.match(event);
	if (cast.hasMatch()) {
		const QString prefix = cast.captured(1).toLower().replace("cast", "_cast_");
		const QString family = cast.captured(2) == "Spell" ? "spell_" : "summoner_";
		return prefix + family + cast.captured(3);
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
			lol_binding binding{action, parts.join('+'), parts.takeLast(), parts, false};
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
	const auto found = by_chord_.constFind(parts.join('+'));
	return found == by_chord_.cend() || found->ambiguous ? nullptr : &found.value();
}
qsizetype lol_input_bindings::size() const
{
	return by_chord_.size();
}
} // namespace sources
