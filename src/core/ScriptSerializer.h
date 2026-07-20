#ifndef SCRIPTSERIALIZER_H
#define SCRIPTSERIALIZER_H

#include <QObject>
#include "ScriptDocument.h"

// ============================================================================
// 脚本序列化器
// 提供脚本文件的导入、导出、验证功能
// ============================================================================
class ScriptSerializer : public QObject
{
    Q_OBJECT

public:
    explicit ScriptSerializer(QObject* parent = nullptr);

    // 保存脚本到文件
    bool save(ScriptDocument& doc, const QString& filePath,
              QString* errorMsg = nullptr);

    // 从文件加载脚本
    bool load(const QString& filePath, ScriptDocument& outDoc,
              QString* errorMsg = nullptr);

    // 验证脚本文件 (不加载事件，只检查结构)
    bool validateFile(const QString& filePath, QString* errorMsg = nullptr);

    // 创建默认脚本文件名
    static QString defaultFileName();

    // 文件过滤器
    static QString fileFilter();

signals:
    void saveCompleted(const QString& path);
    void loadCompleted(const QString& path);
    void errorOccurred(const QString& message);
};

#endif // SCRIPTSERIALIZER_H
