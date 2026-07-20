#ifndef SCRIPTEVENTTABLEMODEL_H
#define SCRIPTEVENTTABLEMODEL_H

#include <QAbstractTableModel>
#include "ScriptDocument.h"

// ============================================================================
// 脚本事件表格Model —— 用于 QTableView 显示和编辑脚本事件
// ============================================================================
class ScriptEventTableModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Column {
        ColIndex = 0,
        ColTime,
        ColType,
        ColButton,
        ColMonitor,
        ColDesktopPos,
        ColRelativePos,
        ColParams,
        ColEnabled,
        ColumnCount  // 必须是最后一个
    };

    explicit ScriptEventTableModel(QObject* parent = nullptr);

    // QAbstractTableModel 接口
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;

    // 数据管理
    void setDocument(ScriptDocument* doc);
    ScriptDocument* document() const { return m_document; }
    ScriptEvent* eventAt(int row);

    // 操作
    void refreshAll();
    void insertEvent(int row, const ScriptEvent& ev);
    void removeRows(QList<int> rows);
    void moveRowUp(int row);
    void moveRowDown(int row);
    void toggleEnabled(int row);

signals:
    void documentModified();

private:
    ScriptDocument* m_document = nullptr;
};

#endif // SCRIPTEVENTTABLEMODEL_H
