#include "AboutDialog.h"
#include <QApplication>
#include <QClipboard>
#include <QTextBrowser>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QVBoxLayout>
AboutDialog::AboutDialog(QWidget* parent) : QDialog(parent)
{
    setWindowTitle("关于 KeyMouseMaster");
    const QString version = QCoreApplication::applicationVersion().isEmpty()
        ? QStringLiteral("1.0.0") : QCoreApplication::applicationVersion();
#ifdef QT_DEBUG
    const QString buildMode = QStringLiteral("Debug");
#else
    const QString buildMode = QStringLiteral("Release");
#endif
    m_information = QStringLiteral("KeyMouseMaster\n键鼠大师\n\nVersion: %1\n\n基于 Qt 开发的 Windows\n键盘、鼠标自动化与脚本录制工具\n\nGitHub:\n%2\n\nQt Version: %3\nBuild: %4 / %5 %6\nLicense: MIT")
        .arg(version, QString::fromLatin1(KmmProjectUrl), QString::fromLatin1(qVersion()),
             buildMode, QString::fromLatin1(__DATE__), QString::fromLatin1(__TIME__));
    auto* layout = new QVBoxLayout(this);
    auto* text = new QTextBrowser(this);
    text->setObjectName("aboutInformation");
    text->setPlainText(m_information);
    text->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    layout->addWidget(text);
    auto* buttons = new QDialogButtonBox(this);
    auto* copy = buttons->addButton("复制信息", QDialogButtonBox::ActionRole);
    copy->setObjectName("copyInformation");
    connect(copy, &QPushButton::clicked, this, [this] { QApplication::clipboard()->setText(m_information); });
    connect(buttons->addButton("GitHub", QDialogButtonBox::ActionRole), &QPushButton::clicked,
            this, &AboutDialog::githubRequested);
    connect(buttons->addButton("确定", QDialogButtonBox::AcceptRole), &QPushButton::clicked,
            this, &QDialog::accept);
    layout->addWidget(buttons);
    resize(fontMetrics().averageCharWidth() * 65, fontMetrics().lineSpacing() * 24);
}
