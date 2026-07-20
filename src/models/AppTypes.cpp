#include "AppTypes.h"
#include <QKeySequence>
#include <QStringList>

bool KeyInfo::isModifier() const
{
    return qtKey == Qt::Key_Control || qtKey == Qt::Key_Shift
        || qtKey == Qt::Key_Alt || qtKey == Qt::Key_Meta
        || qtKey == Qt::Key_AltGr;
}

QString HotkeyInfo::toString() const
{
    if (!isValid()) return {};

    QStringList parts;
    if (ctrl)  parts << "Ctrl";
    if (shift) parts << "Shift";
    if (alt)   parts << "Alt";
    if (win)   parts << "Win";

    QKeySequence ks(static_cast<Qt::Key>(key));
    parts << ks.toString();

    return parts.join(" + ");
}
