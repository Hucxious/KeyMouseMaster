#include "ScriptSerializer.h"
#include "Logger.h"
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>

ScriptSerializer::ScriptSerializer(QObject* parent)
    : QObject(parent)
{
}

bool ScriptSerializer::save(ScriptDocument& doc, const QString& filePath,
                              QString* errorMsg)
{
    QString path = filePath;
    if (!path.endsWith(".kms", Qt::CaseInsensitive))
        path += ".kms";

    bool ok = doc.saveToFile(path, errorMsg);
    if (ok) {
        LOG_INFO(QString("脚本已保存: %1").arg(path));
        emit saveCompleted(path);
    } else {
        LOG_ERROR(QString("脚本保存失败: %1").arg(errorMsg ? *errorMsg : "未知错误"));
        emit errorOccurred(errorMsg ? *errorMsg : "保存失败");
    }
    return ok;
}

bool ScriptSerializer::load(const QString& filePath, ScriptDocument& outDoc,
                              QString* errorMsg)
{
    if (!QFile::exists(filePath)) {
        if (errorMsg) *errorMsg = QString("文件不存在: %1").arg(filePath);
        return false;
    }

    bool ok = outDoc.loadFromFile(filePath, errorMsg);
    if (ok) {
        LOG_INFO(QString("脚本已加载: %1 (%2 个事件)")
            .arg(filePath).arg(outDoc.eventCount()));
        emit loadCompleted(filePath);
    } else {
        LOG_ERROR(QString("脚本加载失败: %1").arg(errorMsg ? *errorMsg : "未知错误"));
        emit errorOccurred(errorMsg ? *errorMsg : "加载失败");
    }
    return ok;
}

bool ScriptSerializer::validateFile(const QString& filePath, QString* errorMsg)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorMsg) *errorMsg = QString("无法打开文件: %1").arg(file.errorString());
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        if (errorMsg) *errorMsg = QString("JSON格式错误: %1").arg(parseError.errorString());
        return false;
    }

    QJsonObject obj = doc.object();

    if (obj["format"].toString() != AppConstants::SCRIPT_FORMAT) {
        if (errorMsg) *errorMsg = "不是有效的KeyMouseMaster脚本文件";
        return false;
    }

    int version = obj["version"].toInt(-1);
    if (version != AppConstants::SCRIPT_VERSION) {
        if (errorMsg) *errorMsg = QString("脚本版本 %1 不受支持").arg(version);
        return false;
    }

    QJsonArray events = obj["events"].toArray();
    if (events.size() > AppConstants::MAX_SCRIPT_EVENTS) {
        if (errorMsg) *errorMsg = QString("事件数量(%1)超过上限").arg(events.size());
        return false;
    }

    return true;
}

QString ScriptSerializer::defaultFileName()
{
    return QString("script_%1.kms")
        .arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss"));
}

QString ScriptSerializer::fileFilter()
{
    return "键鼠大师脚本 (*.kms);;所有文件 (*.*)";
}
