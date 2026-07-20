#ifndef SCRIPTDOCUMENT_H
#define SCRIPTDOCUMENT_H

#include "ScriptEvent.h"
#include "MonitorInfo.h"
#include <QString>
#include <QVector>
#include <QDateTime>
#include <QJsonObject>

// ============================================================================
// 脚本文档 —— 包含完整脚本数据
// ============================================================================
class ScriptDocument
{
public:
    ScriptDocument();

    // 元数据
    QString name;
    QString description;
    QDateTime createdAt;
    QDateTime modifiedAt;
    QString filePath;              // 关联的文件路径 (未保存时为空)

    // 录制时桌面及显示器信息
    QRect virtualDesktopBounds;
    QVector<MonitorInfo> monitors;

    // 事件列表
    QVector<ScriptEvent> events;

    // 回放和录制设置
    PlaybackSettings playbackSettings;
    RecordingSettings recordingSettings;
    CoordinateMode coordinateMode = CoordinateMode::MonitorRelative;

    // 序列化
    QJsonObject toJson() const;
    bool fromJson(const QJsonObject& obj, QString* errorMsg = nullptr);

    // 导入导出
    bool saveToFile(const QString& path, QString* errorMsg = nullptr);
    bool loadFromFile(const QString& path, QString* errorMsg = nullptr);

    // 验证
    bool isValid(QString* errorMsg = nullptr) const;

    // 工具方法
    void clear();
    void addEvent(const ScriptEvent& ev);
    void removeEvent(int index);
    void moveEventUp(int index);
    void moveEventDown(int index);
    void toggleEventEnabled(int index);
    int  eventCount() const { return events.size(); }
    bool isEmpty() const { return events.isEmpty(); }

    // 查找事件中引用的显示器是否存在
    bool validateMonitors(const QVector<MonitorInfo>& currentMonitors,
                          QString* errorMsg = nullptr) const;

    // 修改时间戳
    void markModified();

private:
    bool validateJsonStructure(const QJsonObject& obj, QString* errorMsg) const;
};

#endif // SCRIPTDOCUMENT_H
