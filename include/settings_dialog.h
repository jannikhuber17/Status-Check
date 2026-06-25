#pragma once
#include <QDialog>

class QLabel;

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget* parent = nullptr);
    void refreshHotkeyLabels();

private:
    QLabel* m_lblCycle    = nullptr;
    QLabel* m_lblNotReady = nullptr;
    QLabel* m_lblStandby  = nullptr;
    QLabel* m_lblReady    = nullptr;
};
