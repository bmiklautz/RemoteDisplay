#include "remotedisplaywidget.h"
#include "remotedisplaywidget_p.h"
#include "freerdpclient.h"
#include "cursorchangenotifier.h"
#include "remotescreenbuffer.h"
#include "scaledscreenbuffer.h"
#include "letterboxedscreenbuffer.h"

#include <QDebug>
#include <QThread>
#include <QPointer>
#include <QPaintEvent>
#include <QPainter>
#include <QTimer>

#define FRAMERATE_LIMIT 40

RemoteDisplayWidgetPrivate::RemoteDisplayWidgetPrivate(RemoteDisplayWidget *q)
    : q_ptr(q), repaintNeeded(false) {
    processorThread = new QThread(q);
    processorThread->start();
}

QPoint RemoteDisplayWidgetPrivate::mapToRemoteDesktop(const QPoint &local) const {
    QPoint remote;
    if (scaledScreenBuffer && letterboxedScreenBuffer) {
        remote = scaledScreenBuffer->mapToSource(
                    letterboxedScreenBuffer->mapToSource(local));
    }
    return remote;
}

void RemoteDisplayWidgetPrivate::resizeScreenBuffers() {
    Q_Q(RemoteDisplayWidget);
    if (scaledScreenBuffer) {
        scaledScreenBuffer->scaleToFit(q->size());
    }
    if (letterboxedScreenBuffer) {
        letterboxedScreenBuffer->resize(q->size());
    }
}

void RemoteDisplayWidgetPrivate::onAboutToConnect() {
    qDebug() << "ON CONNECT";
}

void RemoteDisplayWidgetPrivate::onConnected() {
    qDebug() << "ON CONNECTED";
    auto bpp = eventProcessor->getDesktopBpp();
    auto width = desktopSize.width();
    auto height = desktopSize.height();

    remoteScreenBuffer = new RemoteScreenBuffer(width, height, bpp, this);
    scaledScreenBuffer = new ScaledScreenBuffer(remoteScreenBuffer, this);
    letterboxedScreenBuffer = new LetterboxedScreenBuffer(scaledScreenBuffer, this);

    eventProcessor->setBitmapRectangleSink(remoteScreenBuffer);

    resizeScreenBuffers();
}

void RemoteDisplayWidgetPrivate::onDisconnected() {
    Q_Q(RemoteDisplayWidget);
    qDebug() << "ON DISCONNECTED";
    emit q->disconnected();
}

void RemoteDisplayWidgetPrivate::onCursorChanged(const QCursor &cursor) {
    Q_Q(RemoteDisplayWidget);
    q->setCursor(cursor);
}

void RemoteDisplayWidgetPrivate::onDesktopUpdated() {
    repaintNeeded = true;
}

void RemoteDisplayWidgetPrivate::onRepaintTimeout() {
    Q_Q(RemoteDisplayWidget);
    if (repaintNeeded) {
        repaintNeeded = false;
        q->repaint();
    }
}

typedef RemoteDisplayWidgetPrivate Pimpl;

RemoteDisplayWidget::RemoteDisplayWidget(QWidget *parent)
    : QWidget(parent), d_ptr(new RemoteDisplayWidgetPrivate(this)) {
    Q_D(RemoteDisplayWidget);
    qRegisterMetaType<Qt::MouseButton>("Qt::MouseButton");

    setAttribute(Qt::WA_OpaquePaintEvent);
    setAttribute(Qt::WA_NoSystemBackground);
    setMouseTracking(true);

    auto cursorNotifier = new CursorChangeNotifier(this);
    connect(cursorNotifier, SIGNAL(cursorChanged(QCursor)), d, SLOT(onCursorChanged(QCursor)));

    d->eventProcessor = new FreeRdpClient(cursorNotifier);
    d->eventProcessor->moveToThread(d->processorThread);

    connect(d->eventProcessor, SIGNAL(aboutToConnect()), d, SLOT(onAboutToConnect()));
    connect(d->eventProcessor, SIGNAL(connected()), d, SLOT(onConnected()));
    connect(d->eventProcessor, SIGNAL(disconnected()), d, SLOT(onDisconnected()));
    connect(d->eventProcessor, SIGNAL(desktopUpdated()), d, SLOT(onDesktopUpdated()));

    auto timer = new QTimer(this);
    timer->setSingleShot(false);
    timer->setInterval(1000 / FRAMERATE_LIMIT);
    connect(timer, SIGNAL(timeout()), d, SLOT(onRepaintTimeout()));
    timer->start();
}

void RemoteDisplayWidget::disconnect() {
    Q_D(RemoteDisplayWidget);
    if (d->eventProcessor) {
        QMetaObject::invokeMethod(d->eventProcessor, "requestStop");
		}
}


RemoteDisplayWidget::~RemoteDisplayWidget() {
    Q_D(RemoteDisplayWidget);
		disconnect();
    d->processorThread->quit();
 	  d->processorThread->wait();
    delete d_ptr;
}

void RemoteDisplayWidget::setDesktopSize(quint16 width, quint16 height) {
    Q_D(RemoteDisplayWidget);
    d->desktopSize = QSize(width, height);
    QMetaObject::invokeMethod(d->eventProcessor, "setSettingDesktopSize",
        Q_ARG(quint16, width), Q_ARG(quint16, height));
}

void RemoteDisplayWidget::connectToHost(const QString &host, quint16 port) {
    Q_D(RemoteDisplayWidget);

    QMetaObject::invokeMethod(d->eventProcessor, "setSettingServerHostName",
        Q_ARG(QString, host));
    QMetaObject::invokeMethod(d->eventProcessor, "setSettingServerPort",
        Q_ARG(quint16, port));

    qDebug() << "Connecting to" << host << ":" << port;
    QMetaObject::invokeMethod(d->eventProcessor, "run");
}

QSize RemoteDisplayWidget::sizeHint() const {
    Q_D(const RemoteDisplayWidget);
    if (d->desktopSize.isValid()) {
        return d->desktopSize;
    }
    return QWidget::sizeHint();
}

void RemoteDisplayWidget::paintEvent(QPaintEvent *event) {
    Q_D(RemoteDisplayWidget);
    if (d->letterboxedScreenBuffer) {
        auto image = d->letterboxedScreenBuffer->createImage();
        if (!image.isNull()) {
            QPainter painter(this);
            painter.drawImage(rect(), image);
        }
    }
}

void RemoteDisplayWidget::mouseMoveEvent(QMouseEvent *event) {
    Q_D(RemoteDisplayWidget);
    d->eventProcessor->sendMouseMoveEvent(d->mapToRemoteDesktop(event->pos()));
}

void RemoteDisplayWidget::mousePressEvent(QMouseEvent *event) {
    Q_D(RemoteDisplayWidget);
    d->eventProcessor->sendMousePressEvent(event->button(),
        d->mapToRemoteDesktop(event->pos()));
}

void RemoteDisplayWidget::mouseReleaseEvent(QMouseEvent *event) {
    Q_D(RemoteDisplayWidget);
    d->eventProcessor->sendMouseReleaseEvent(event->button(),
        d->mapToRemoteDesktop(event->pos()));
}

void RemoteDisplayWidget::keyPressEvent(QKeyEvent *event) {
    Q_D(RemoteDisplayWidget);
    d->eventProcessor->sendKeyEvent(event);
    event->accept();
}

void RemoteDisplayWidget::keyReleaseEvent(QKeyEvent *event) {
    Q_D(RemoteDisplayWidget);
    d->eventProcessor->sendKeyEvent(event);
    event->accept();
}

void RemoteDisplayWidget::resizeEvent(QResizeEvent *event) {
    Q_D(RemoteDisplayWidget);
    d->resizeScreenBuffers();
    QWidget::resizeEvent(event);
}

void RemoteDisplayWidget::closeEvent(QCloseEvent *event) {
	disconnect();
	event->accept();
}
