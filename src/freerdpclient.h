#ifndef FREERDPCLIENT_H
#define FREERDPCLIENT_H

#include <QPointer>
#include <QWidget>
#include <freerdp/freerdp.h>
#include <freerdp/gdi/gdi.h>

class FreeRdpEventLoop;
class Cursor;
class BitmapRectangleSink;
class PointerChangeSink;
class ScreenBuffer;

class FreeRdpClient : public QObject {
    Q_OBJECT
public:
    FreeRdpClient(PointerChangeSink *pointerSink);
    ~FreeRdpClient();

    void setBitmapRectangleSink(BitmapRectangleSink *sink);

    quint8 getDesktopBpp() const;

    void sendMouseMoveEvent(const QPoint &pos);
    void sendMousePressEvent(Qt::MouseButton button, const QPoint &pos);
    void sendMouseReleaseEvent(Qt::MouseButton button, const QPoint &pos);
    void sendKeyEvent(QKeyEvent *event);

public slots:
    void setSettingServerHostName(const QString &host);
    void setSettingServerPort(quint16 port);
    void setSettingDesktopSize(quint16 width, quint16 height);

    void run();
    void requestStop();

signals:
    void aboutToConnect();
    void connected();
    void disconnected();
    void desktopUpdated();

private:
    void initFreeRDP();
    void sendMouseEvent(UINT16 flags, const QPoint &pos);
    void addStaticChannel(const QStringList& args);

    static BOOL BitmapUpdateCallback(rdpContext *context, BITMAP_UPDATE *updates);
    static BOOL EndPaintCallback(rdpContext *context);
    static BOOL BeginPaintCallback(rdpContext *context);
    static BOOL PreConnectCallback(freerdp* instance);
    static BOOL PostConnectCallback(freerdp* instance);
    static void PostDisconnectCallback(freerdp* instance);
    static int ReceiveChannelDataCallback(freerdp* instance, int channelId,
        BYTE* data, int size, int flags, int total_size);

    static BOOL PointerNewCallback(rdpContext* context, rdpPointer* pointer);
    static void PointerFreeCallback(rdpContext* context, rdpPointer* pointer);
    static BOOL PointerSetCallback(rdpContext* context, rdpPointer* pointer);
    static BOOL PointerSetNullCallback(rdpContext* context);
    static BOOL PointerSetDefaultCallback(rdpContext* context);

    freerdp* freeRdpInstance;
    BitmapRectangleSink *bitmapRectangleSink;
    PointerChangeSink *pointerChangeSink;
    QPointer<FreeRdpEventLoop> loop;
    static int instanceCount;
};

#endif // FREERDPCLIENT_H
