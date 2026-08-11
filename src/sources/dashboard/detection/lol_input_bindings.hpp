#pragma once

#include <QHash>
#include <QString>
#include <QStringList>

namespace sources {
struct lol_binding {
	QString action;
	QString chord;
	QString trigger;
	QStringList modifiers;
	bool ambiguous{};
};

class lol_input_bindings {
public:
	bool parse(const QString &contents, const QString &champion = {});
	const lol_binding *resolve(const QString &trigger, const QStringList &modifiers) const;
	qsizetype size() const;

private:
	QHash<QString, lol_binding> by_chord_;
};
} // namespace sources
