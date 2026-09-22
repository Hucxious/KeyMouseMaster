#ifndef ABOUTDIALOG_H
#define ABOUTDIALOG_H
#include <QDialog>
inline constexpr char KmmProjectUrl[] = "https://github.com/Hucxious/KeyMouseMaster";
class AboutDialog : public QDialog {
    Q_OBJECT
public:
    explicit AboutDialog(QWidget* parent = nullptr);
    QString information() const { return m_information; }
signals:
    void githubRequested();
private:
    QString m_information;
};
#endif
