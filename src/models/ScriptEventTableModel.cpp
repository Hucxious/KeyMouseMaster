#include "ScriptEventTableModel.h"
#include <QColor>
#include <QBrush>

ScriptEventTableModel::ScriptEventTableModel(QObject* parent)
    : QAbstractTableModel(parent)
{
}

int ScriptEventTableModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) return 0;
    return m_document ? m_document->eventCount() : 0;
}

int ScriptEventTableModel::columnCount(const QModelIndex& parent) const
{
    if (parent.isValid()) return 0;
    return ColumnCount;
}

QVariant ScriptEventTableModel::data(const QModelIndex& index, int role) const
{
    if (!m_document || !index.isValid())
        return QVariant();

    int row = index.row();
    if (row < 0 || row >= m_document->eventCount())
        return QVariant();

    const ScriptEvent& ev = m_document->events[row];

    if (role == Qt::DisplayRole || role == Qt::EditRole) {
        switch (index.column()) {
        case ColIndex:    return ev.eventIndex + 1;
        case ColTime:     return QString::number(ev.timestampMs);
        case ColType:     return scriptEventTypeToString(ev.type);
        case ColButton:   return ev.isMouseEvent() ? mouseButtonToString(ev.mouseButton) : ev.keyDisplayName;
        case ColMonitor:  return ev.monitorDeviceName;
        case ColDesktopPos: return QString("(%1, %2)").arg(ev.virtualDesktopPos.x()).arg(ev.virtualDesktopPos.y());
        case ColRelativePos: return QString("(%1, %2)").arg(ev.monitorInternalPos.x()).arg(ev.monitorInternalPos.y());
        case ColParams:
            if (ev.type == ScriptEventType::MouseWheel || ev.type == ScriptEventType::MouseHWheel)
                return QString("滚轮: %1").arg(ev.wheelDelta);
            return QVariant();
        case ColEnabled:  return ev.enabled ? "是" : "否";
        }
    }

    if (role == Qt::ForegroundRole && !ev.enabled) {
        return QBrush(QColor(160, 160, 160));
    }

    if (role == Qt::TextAlignmentRole) {
        if (index.column() == ColIndex || index.column() == ColEnabled)
            return Qt::AlignCenter;
    }

    return QVariant();
}

QVariant ScriptEventTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return QVariant();

    switch (section) {
    case ColIndex:        return "序号";
    case ColTime:         return "时间(ms)";
    case ColType:         return "类型";
    case ColButton:       return "按键/按钮";
    case ColMonitor:      return "显示器";
    case ColDesktopPos:   return "桌面坐标";
    case ColRelativePos:  return "相对坐标";
    case ColParams:       return "参数";
    case ColEnabled:      return "启用";
    }
    return QVariant();
}

Qt::ItemFlags ScriptEventTableModel::flags(const QModelIndex& index) const
{
    Qt::ItemFlags f = QAbstractTableModel::flags(index);
    if (index.column() == ColEnabled)
        f |= Qt::ItemIsEditable;
    if (index.column() == ColTime)
        f |= Qt::ItemIsEditable;
    return f;
}

bool ScriptEventTableModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (!m_document || !index.isValid() || role != Qt::EditRole)
        return false;

    int row = index.row();
    if (row < 0 || row >= m_document->eventCount())
        return false;

    ScriptEvent& ev = m_document->events[row];
    bool changed = false;

    switch (index.column()) {
    case ColEnabled:
        ev.enabled = value.toBool();
        changed = true;
        break;
    case ColTime: {
        qint64 t = value.toLongLong();
        if (t >= 0) { ev.timestampMs = t; changed = true; }
        break;
    }
    }

    if (changed) {
        m_document->markModified();
        emit dataChanged(index, index);
        emit documentModified();
    }
    return changed;
}

void ScriptEventTableModel::setDocument(ScriptDocument* doc)
{
    beginResetModel();
    m_document = doc;
    endResetModel();
}

ScriptEvent* ScriptEventTableModel::eventAt(int row)
{
    if (!m_document || row < 0 || row >= m_document->eventCount())
        return nullptr;
    return &m_document->events[row];
}

void ScriptEventTableModel::refreshAll()
{
    beginResetModel();
    endResetModel();
}

void ScriptEventTableModel::insertEvent(int row, const ScriptEvent& ev)
{
    if (!m_document) return;
    beginInsertRows(QModelIndex(), row, row);
    m_document->events.insert(row, ev);
    for (int i = 0; i < m_document->eventCount(); ++i)
        m_document->events[i].eventIndex = i;
    m_document->markModified();
    endInsertRows();
    emit documentModified();
}

void ScriptEventTableModel::removeRows(QList<int> rows)
{
    if (!m_document || rows.isEmpty()) return;
    std::sort(rows.begin(), rows.end(), std::greater<int>());
    for (int row : rows) {
        if (row >= 0 && row < m_document->eventCount()) {
            beginRemoveRows(QModelIndex(), row, row);
            m_document->events.removeAt(row);
            endRemoveRows();
        }
    }
    for (int i = 0; i < m_document->eventCount(); ++i)
        m_document->events[i].eventIndex = i;
    m_document->markModified();
    emit documentModified();
}

void ScriptEventTableModel::moveRowUp(int row)
{
    if (!m_document || row <= 0 || row >= m_document->eventCount()) return;
    beginMoveRows(QModelIndex(), row, row, QModelIndex(), row - 1);
    m_document->moveEventUp(row);
    endMoveRows();
    emit documentModified();
}

void ScriptEventTableModel::moveRowDown(int row)
{
    if (!m_document || row < 0 || row >= m_document->eventCount() - 1) return;
    beginMoveRows(QModelIndex(), row, row, QModelIndex(), row + 2);
    m_document->moveEventDown(row);
    endMoveRows();
    emit documentModified();
}

void ScriptEventTableModel::toggleEnabled(int row)
{
    if (!m_document || row < 0 || row >= m_document->eventCount()) return;
    m_document->toggleEventEnabled(row);
    QModelIndex idx = index(row, ColEnabled);
    emit dataChanged(idx, idx);
    emit documentModified();
}
