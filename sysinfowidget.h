#pragma once

#include <QWidget>
#include <QTimer>
#include <QMouseEvent>
#include <QPoint>
#include <QVector>

class SysInfoWidget : public QWidget
{
    Q_OBJECT

public:
    explicit SysInfoWidget(QWidget *parent = nullptr);

    void showOnPrimaryScreen();

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private slots:
    void refreshMetrics();

private:
    struct CpuSnapshot {
        qint64 idle = 0;
        qint64 total = 0;
        bool valid = false;
    };

    bool readCpuSnapshot(CpuSnapshot &snap) const;
    QString uptimeText() const;
    void positionBottomRight();

private:
    QTimer m_timer;
    CpuSnapshot m_prevCpu;

    double m_cpuUsage = 0.0;
    double m_memUsage = 0.0;
    double m_diskUsage = 0.0;

    qint64 m_diskTotalBytes = 0;
    qint64 m_diskUsedBytes = 0;

    qint64 m_memTotalBytes = 0;
    qint64 m_memUsedBytes = 0;
    qint64 m_memAvailableBytes = 0;

    QString m_netName = "offline";
    QString m_netAddr = "-";
    QString m_uptime = "-";

    QVector<double> m_cpuHistory;
    int m_cpuHistoryMax = 60;

    QPoint m_dragOffset;
    bool m_dragging = false;
};