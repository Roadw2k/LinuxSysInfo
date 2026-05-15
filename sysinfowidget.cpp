#include "sysinfowidget.h"

#include <QAbstractSocket>
#include <QFile>
#include <QFont>
#include <QGuiApplication>
#include <QHostAddress>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QNetworkAddressEntry>
#include <QNetworkInterface>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QRegularExpression>
#include <QScreen>
#include <QStorageInfo>
#include <QTextStream>
#include <QWindow>
#include <QtGlobal>

static qint64 meminfoValueKb(const QString &content, const QString &key)
{
    const QRegularExpression re(
        QStringLiteral("^%1:\\s+(\\d+)\\s+kB$")
            .arg(QRegularExpression::escape(key)),
        QRegularExpression::MultilineOption);

    const auto match = re.match(content);
    if (!match.hasMatch())
        return -1;

    return match.captured(1).toLongLong();
}

static QString humanBytes(qint64 bytes)
{
    const double gb = bytes / (1024.0 * 1024.0 * 1024.0);
    return QString::number(gb, 'f', 1) + " GB";
}

SysInfoWidget::SysInfoWidget(QWidget *parent)
    : QWidget(parent)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnBottomHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_NoSystemBackground);
    setFocusPolicy(Qt::NoFocus);
    setCursor(Qt::OpenHandCursor);

    setFixedSize(420, 280);

    connect(&m_timer, &QTimer::timeout, this, &SysInfoWidget::refreshMetrics);
    m_timer.setTimerType(Qt::CoarseTimer);
    m_timer.start(1000);

    refreshMetrics();
}

void SysInfoWidget::showOnPrimaryScreen()
{
    show();
    raise();
    positionBottomRight();
}

void SysInfoWidget::positionBottomRight()
{
    QWindow *win = windowHandle();
    QScreen *scr = win ? win->screen() : QGuiApplication::primaryScreen();
    if (!scr)
        return;

    const QRect geo = scr->availableGeometry();
    const int margin = 24;

    move(geo.right() - width() - margin,
         geo.bottom() - height() - margin);
}

void SysInfoWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    positionBottomRight();
}

void SysInfoWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = true;
        m_dragOffset = event->globalPosition().toPoint() - frameGeometry().topLeft();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }

    QWidget::mousePressEvent(event);
}

void SysInfoWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragging && (event->buttons() & Qt::LeftButton)) {
        move(event->globalPosition().toPoint() - m_dragOffset);
        event->accept();
        return;
    }

    QWidget::mouseMoveEvent(event);
}

void SysInfoWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = false;
        setCursor(Qt::OpenHandCursor);
        event->accept();
        return;
    }

    QWidget::mouseReleaseEvent(event);
}

bool SysInfoWidget::readCpuSnapshot(CpuSnapshot &snap) const
{
    QFile file("/proc/stat");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    QTextStream in(&file);
    const QString line = in.readLine();
    const QStringList parts = line.simplified().split(' ');

    if (parts.size() < 5 || parts.first() != "cpu")
        return false;

    const qint64 user = parts.value(1).toLongLong();
    const qint64 nice = parts.value(2).toLongLong();
    const qint64 system = parts.value(3).toLongLong();
    const qint64 idle = parts.value(4).toLongLong();
    const qint64 iowait = parts.value(5).toLongLong();
    const qint64 irq = parts.value(6).toLongLong();
    const qint64 softirq = parts.value(7).toLongLong();
    const qint64 steal = parts.value(8).toLongLong();

    snap.idle = idle + iowait;
    snap.total = user + nice + system + idle + iowait + irq + softirq + steal;
    snap.valid = true;
    return true;
}

QString SysInfoWidget::uptimeText() const
{
    QFile file("/proc/uptime");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return "-";

    QTextStream in(&file);
    double seconds = 0.0;
    in >> seconds;

    const qint64 total = qint64(seconds);
    const qint64 days = total / 86400;
    const qint64 hours = (total % 86400) / 3600;
    const qint64 minutes = (total % 3600) / 60;

    if (days > 0)
        return QString("%1d %2h %3m").arg(days).arg(hours).arg(minutes);

    return QString("%1h %2m").arg(hours).arg(minutes);
}

void SysInfoWidget::refreshMetrics()
{
    CpuSnapshot now;
    if (readCpuSnapshot(now) && m_prevCpu.valid) {
        const qint64 deltaTotal = now.total - m_prevCpu.total;
        const qint64 deltaIdle = now.idle - m_prevCpu.idle;

        if (deltaTotal > 0) {
            m_cpuUsage = 100.0 * double(deltaTotal - deltaIdle) / double(deltaTotal);
        }
    }
    m_prevCpu = now;

    m_cpuHistory.append(m_cpuUsage);
    while (m_cpuHistory.size() > m_cpuHistoryMax) {
        m_cpuHistory.removeFirst();
    }

    qint64 memTotalKb = -1;
    qint64 memAvailKb = -1;

    QFile memFile("/proc/meminfo");
    if (memFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        const QString content = QString::fromUtf8(memFile.readAll());

        memTotalKb = meminfoValueKb(content, "MemTotal");
        memAvailKb = meminfoValueKb(content, "MemAvailable");

        if (memAvailKb < 0) {
            const qint64 memFreeKb = meminfoValueKb(content, "MemFree");
            const qint64 buffersKb = meminfoValueKb(content, "Buffers");
            const qint64 cachedKb = meminfoValueKb(content, "Cached");
            const qint64 sreclaimableKb = meminfoValueKb(content, "SReclaimable");
            const qint64 shmemKb = meminfoValueKb(content, "Shmem");

            if (memFreeKb >= 0 && buffersKb >= 0 && cachedKb >= 0 &&
                sreclaimableKb >= 0 && shmemKb >= 0) {
                memAvailKb = memFreeKb + buffersKb + cachedKb + sreclaimableKb - shmemKb;
            }
        }
    }

    if (memTotalKb > 0 && memAvailKb >= 0) {
        memAvailKb = qMax<qint64>(0, memAvailKb);
        const qint64 memUsedKb = qMax<qint64>(0, memTotalKb - memAvailKb);

        m_memTotalBytes = memTotalKb * 1024;
        m_memAvailableBytes = memAvailKb * 1024;
        m_memUsedBytes = memUsedKb * 1024;
        m_memUsage = 100.0 * double(memUsedKb) / double(memTotalKb);
    }

    QStorageInfo root("/");
    if (root.isValid() && root.isReady() && root.bytesTotal() > 0) {
        m_diskTotalBytes = root.bytesTotal();
        m_diskUsedBytes = root.bytesTotal() - root.bytesFree();
        m_diskUsage = 100.0 * double(m_diskUsedBytes) / double(m_diskTotalBytes);
    } else {
        m_diskTotalBytes = 0;
        m_diskUsedBytes = 0;
        m_diskUsage = 0.0;
    }

    m_netName = "offline";
    m_netAddr = "-";

    const auto interfaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface &iface : interfaces) {
        if (!(iface.flags() & QNetworkInterface::IsUp))
            continue;
        if (!(iface.flags() & QNetworkInterface::IsRunning))
            continue;
        if (iface.flags() & QNetworkInterface::IsLoopBack)
            continue;

        for (const QNetworkAddressEntry &entry : iface.addressEntries()) {
            const QHostAddress addr = entry.ip();
            if (addr.protocol() == QAbstractSocket::IPv4Protocol) {
                m_netName = iface.humanReadableName();
                m_netAddr = addr.toString();
                break;
            }
        }

        if (m_netAddr != "-")
            break;
    }

    m_uptime = uptimeText();
    update();
}

void SysInfoWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const QRectF outer = rect().adjusted(1, 1, -1, -1);
    const QRectF shadow1 = outer.translated(0, 4).adjusted(-1, -1, 1, 1);
    const QRectF shadow2 = outer.translated(0, 8).adjusted(-4, -4, 4, 4);

    QPainterPath shadowPath1;
    shadowPath1.addRoundedRect(shadow1, 24, 24);
    p.fillPath(shadowPath1, QColor(0, 0, 0, 60));

    QPainterPath shadowPath2;
    shadowPath2.addRoundedRect(shadow2, 28, 28);
    p.fillPath(shadowPath2, QColor(0, 0, 0, 28));

    QPainterPath panelPath;
    panelPath.addRoundedRect(outer, 24, 24);

    QLinearGradient bg(outer.topLeft(), outer.bottomRight());
    bg.setColorAt(0.0, QColor(28, 28, 34, 238));
    bg.setColorAt(1.0, QColor(18, 18, 22, 238));
    p.fillPath(panelPath, bg);

    p.setPen(QPen(QColor(255, 255, 255, 24), 1));
    p.drawPath(panelPath);

    const int left = 20;
    const int top = 16;
    const int labelW = 78;
    const int valueX = left + labelW;
    const int contentRight = width() - left;
    const int graphW = 146;
    const int graphH = 42;

    QFont titleFont = font();
    titleFont.setPointSize(titleFont.pointSize() + 4);
    titleFont.setBold(true);

    QFont labelFont = font();
    labelFont.setPointSize(labelFont.pointSize());

    QFont valueFont = font();
    valueFont.setPointSize(valueFont.pointSize() + 1);
    valueFont.setBold(true);

    QFont subFont = font();
    subFont.setPointSize(subFont.pointSize() - 1);

    const QColor titleColor(255, 255, 255, 245);
    const QColor labelColor(255, 255, 255, 155);
    const QColor valueColor(255, 255, 255, 240);
    const QColor subColor(255, 255, 255, 175);
    const QColor accent(86, 180, 255, 235);
    const QColor accentFill(86, 180, 255, 45);

    p.setPen(titleColor);
    p.setFont(titleFont);
    p.drawText(QRect(left, top, width() - left * 2, 28),
               Qt::AlignLeft | Qt::AlignVCenter,
               "System Info");

    const int titleBarY = top + 28;
    p.setPen(QPen(QColor(255, 255, 255, 18), 1));
    p.drawLine(left, titleBarY + 6, width() - left, titleBarY + 6);

    auto drawOneLineRow = [&](int y, const QString &label, const QString &value) {
        p.setFont(labelFont);
        p.setPen(labelColor);
        p.drawText(QRect(left, y, labelW, 24),
                   Qt::AlignLeft | Qt::AlignVCenter,
                   label);

        p.setFont(valueFont);
        p.setPen(valueColor);
        p.drawText(QRect(valueX, y, contentRight - valueX, 24),
                   Qt::AlignLeft | Qt::AlignVCenter,
                   value);
    };

    auto drawCpuRow = [&](int y) {
        p.setFont(labelFont);
        p.setPen(labelColor);
        p.drawText(QRect(left, y, labelW, 24),
                   Qt::AlignLeft | Qt::AlignVCenter,
                   "CPU");

        QRect graphRect(contentRight - graphW, y + 2, graphW, graphH);
        QRect valueRect(valueX, y, graphRect.left() - valueX - 8, 24);

        p.setFont(valueFont);
        p.setPen(valueColor);
        p.drawText(valueRect,
                   Qt::AlignLeft | Qt::AlignVCenter,
                   QString("%1%").arg(QString::number(m_cpuUsage, 'f', 1)));

        p.setFont(subFont);
        p.setPen(subColor);
        p.drawText(QRect(valueX, y + 18, valueRect.width(), 16),
                   Qt::AlignLeft | Qt::AlignVCenter,
                   "live usage");

        QPainterPath graphBg;
        graphBg.addRoundedRect(graphRect.adjusted(0, 0, -1, -1), 10, 10);
        p.fillPath(graphBg, QColor(255, 255, 255, 8));
        p.setPen(QPen(QColor(255, 255, 255, 20), 1));
        p.drawPath(graphBg);

        if (m_cpuHistory.size() >= 2) {
            const QRectF inner = graphRect.adjusted(8, 7, -8, -8);

            QPainterPath linePath;
            QPainterPath fillPath;

            for (int i = 0; i < m_cpuHistory.size(); ++i) {
                const double t = double(i) / double(m_cpuHistory.size() - 1);
                const double x = inner.left() + t * inner.width();
                const double v = qBound(0.0, m_cpuHistory.at(i), 100.0);
                const double yy = inner.bottom() - (v / 100.0) * inner.height();

                if (i == 0) {
                    linePath.moveTo(x, yy);
                    fillPath.moveTo(inner.left(), inner.bottom());
                    fillPath.lineTo(x, yy);
                } else {
                    linePath.lineTo(x, yy);
                    fillPath.lineTo(x, yy);
                }
            }

            fillPath.lineTo(inner.right(), inner.bottom());
            fillPath.closeSubpath();

            p.fillPath(fillPath, accentFill);
            p.setPen(QPen(accent, 1.6));
            p.drawPath(linePath);

            const double last = qBound(0.0, m_cpuHistory.last(), 100.0);
            const double lastX = inner.right();
            const double lastY = inner.bottom() - (last / 100.0) * inner.height();
            p.setBrush(accent);
            p.setPen(Qt::NoPen);
            p.drawEllipse(QPointF(lastX, lastY), 2.6, 2.6);
        }
    };

    int y = titleBarY + 14;

    drawCpuRow(y);
    y += 50;

    drawOneLineRow(y, "Memory",
                   QString("%1 / %2 (%3%)")
                        .arg(humanBytes(m_memUsedBytes),
                             humanBytes(m_memTotalBytes),
                             QString::number(m_memUsage, 'f', 1)));
    y += 30;

    drawOneLineRow(y, "Disk",
                   QString("%1 / %2 (%3%)")
                       .arg(humanBytes(m_diskUsedBytes),
                            humanBytes(m_diskTotalBytes),
                            QString::number(m_diskUsage, 'f', 1)));
    y += 30;

    drawOneLineRow(y, "NIC",
                   QString("%1 (%2)").arg(m_netName, m_netAddr));
    y += 30;

    drawOneLineRow(y, "Uptime", m_uptime);

    p.setPen(QPen(QColor(255, 255, 255, 14), 1));
    p.drawLine(left, height() - 26, width() - left, height() - 26);

    p.setFont(subFont);
    p.setPen(QColor(255, 255, 255, 110));
    p.drawText(QRect(left, height() - 22, width() - left * 2, 14),
               Qt::AlignLeft | Qt::AlignVCenter,
               "Made by Roadw2k");
}