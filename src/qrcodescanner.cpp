/**
@copyright Evgeny Sidorov 2022

This software is dual-licensed. Choose the appropriate license for your project.

1. The GNU GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-GPLv3.md](LICENSE-GPLv3.md) or copy at https://www.gnu.org/licenses/gpl-3.0.txt)
    
2. The GNU LESSER GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-LGPLv3.md](LICENSE-LGPLv3.md) or copy at https://www.gnu.org/licenses/lgpl-3.0.txt).

You may select, at your option, one of the above-listed licenses.

*/

/****************************************************************************/

/** @file uise/desktop/src/qrcodescanner.cpp
*
*
*/

/****************************************************************************/

#include <QApplication>
#include <QCamera>
#include <QVideoSink>
#include <QMediaDevices>
#include <QPermission>
#include <QMediaCaptureSession>
#include <QComboBox>

#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGraphicsVideoItem>
#include <QGraphicsPolygonItem>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/utils/destroywidget.hpp>
#include <uise/desktop/style.hpp>
#include <uise/desktop/pushbutton.hpp>
#include <uise/desktop/utils/singleshottimer.hpp>
#include <uise/desktop/qrcodefromvideo.hpp>
#include <uise/desktop/qrcodescanner.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

/************************** QrCodeScanner ****************************/

using namespace qrcode;

//--------------------------------------------------------------------------

class QrCodeScanner_p
{
    public:

        QMediaDevices devices;
        QMediaCaptureSession captureSession;
        std::unique_ptr<QCamera> camera;

        QComboBox* deviceList;

        QGraphicsScene* scene;
        QGraphicsView* view;
        QGraphicsVideoItem* videoPreview=nullptr;
        QGraphicsPolygonItem* qrcodeFrame=nullptr;

        QFrame* buttonsFrame;
        PushButton* startButton;
        PushButton* stopButton;

        qrcode::VideoReader* qrCodeReader;

        bool initialized=false;
        QSize previewSize;

        bool barcodeFound=false;

        QColor qrFrameColor=QrCodeScanner::DefaulQrFrameColor;
        qreal qrFrameWidth=QrCodeScanner::DefaulQrFrameWidth;
        Qt::PenStyle qrFrameStyle=QrCodeScanner::DefaulQrFrameStyle;

        bool running=false;
};

//--------------------------------------------------------------------------

QrCodeScanner::QrCodeScanner(QWidget* parent)
    : AbstractQrCodeScanner(parent),
      pimpl(std::make_unique<QrCodeScanner_p>())
{
    pimpl->previewSize=QSize(DefaultPreviewSize,DefaultPreviewSize);
}

//--------------------------------------------------------------------------

void QrCodeScanner::construct()
{
    auto l=Layout::vertical(this);

    l->addStretch(1);

    pimpl->deviceList=new QComboBox(this);
    pimpl->deviceList->setObjectName("deviceList");
    l->addWidget(pimpl->deviceList,0,Qt::AlignCenter);
    connect(
        pimpl->deviceList,
        &QComboBox::currentIndexChanged,
        this,
        [this](int index)
        {
            auto data=pimpl->deviceList->itemData(index);
            updateCameraDevice(data);
        }
    );

    pimpl->scene=new QGraphicsScene(this);
    pimpl->view=new QGraphicsView(this);
    pimpl->view->setScene(pimpl->scene);
    l->addWidget(pimpl->view,0,Qt::AlignCenter);

    pimpl->videoPreview=new QGraphicsVideoItem();
    pimpl->scene->addItem(pimpl->videoPreview);
    pimpl->qrcodeFrame=new QGraphicsPolygonItem();
    pimpl->scene->addItem(pimpl->qrcodeFrame);
    pimpl->qrcodeFrame->setVisible(false);
    pimpl->qrcodeFrame->setPen(QPen{pimpl->qrFrameColor,pimpl->qrFrameWidth,pimpl->qrFrameStyle});
    pimpl->videoPreview->setSize(pimpl->previewSize);    

    pimpl->buttonsFrame=new QFrame(this);
    pimpl->buttonsFrame->setObjectName("buttonsFrame");
    l->addWidget(pimpl->buttonsFrame);
    auto bl=Layout::horizontal(pimpl->buttonsFrame);
    bl->addStretch(1);
    pimpl->startButton=new PushButton(tr("Start"),pimpl->buttonsFrame);
    pimpl->startButton->setObjectName("startButton");
    bl->addWidget(pimpl->startButton);
    pimpl->stopButton=new PushButton(tr("Stop"),pimpl->buttonsFrame);
    pimpl->stopButton->setObjectName("stopButton");
    bl->addWidget(pimpl->stopButton);
    bl->addStretch(1);

    l->addStretch(1);

    pimpl->qrCodeReader=new VideoReader(this);
    pimpl->qrCodeReader->setVideoSink(pimpl->videoPreview->videoSink());
    connect(
        pimpl->qrCodeReader,
        &VideoReader::foundBarcode,
        this,
        &QrCodeScanner::onFoundBarcode
    );
    connect(
        pimpl->qrCodeReader,
        &VideoReader::failedRead,
        this,
        &QrCodeScanner::onMissedBarcode
    );

    connect(
        pimpl->startButton,
        &PushButton::clicked,
        this,
        &QrCodeScanner::start
    );
    connect(
        pimpl->stopButton,
        &PushButton::clicked,
        this,
        &QrCodeScanner::stop
    );

    connect(
        pimpl->videoPreview->videoSink(),
        &QVideoSink::videoSizeChanged,
        this,
        &QrCodeScanner::onVideoSizeChanged
    );

    updateCameraActive(false);

    updateCameras();
    connect(&pimpl->devices, &QMediaDevices::videoInputsChanged, this, &QrCodeScanner::updateCameras);
}

//--------------------------------------------------------------------------

QrCodeScanner::~QrCodeScanner()
{}

//--------------------------------------------------------------------------

void QrCodeScanner::init()
{
#if QT_CONFIG(permissions)
    QCameraPermission cameraPermission;
    switch (qApp->checkPermission(cameraPermission))
    {
        case Qt::PermissionStatus::Undetermined:
            qApp->requestPermission(cameraPermission, this, &QrCodeScanner::init);
            return;
        case Qt::PermissionStatus::Denied:
            emit displayError(tr("Camera permission is not granted!"));
            return;
        case Qt::PermissionStatus::Granted:
            pimpl->initialized=true;            
            break;
    }
#endif

    setCamera(QMediaDevices::defaultVideoInput());
    start();
}

//--------------------------------------------------------------------------

void QrCodeScanner::setCamera(const QCameraDevice &cameraDevice)
{
    auto running=pimpl->running;
    if (running)
    {
        stop();
    }

    pimpl->camera.reset(new QCamera(cameraDevice));
    pimpl->captureSession.setCamera(pimpl->camera.get());

    connect(pimpl->camera.get(), &QCamera::activeChanged, this, &QrCodeScanner::updateCameraActive);
    connect(pimpl->camera.get(), &QCamera::errorOccurred, this, &QrCodeScanner::displayCameraError);

    pimpl->captureSession.setVideoOutput(pimpl->videoPreview);

    if (running)
    {
        start();
    }
}

//--------------------------------------------------------------------------

void QrCodeScanner::start()
{
    if (!pimpl->initialized)
    {
        init();
    }

    if (pimpl->initialized && pimpl->camera)
    {
        pimpl->camera->start();
    }
}

//--------------------------------------------------------------------------

void QrCodeScanner::stop()
{
    if (pimpl->camera)
    {
        pimpl->camera->stop();
    }
}

//--------------------------------------------------------------------------

void QrCodeScanner::updateCameraActive(bool active)
{
    pimpl->running=active;
    if (active)
    {
        pimpl->startButton->setEnabled(false);
        pimpl->stopButton->setEnabled(true);
        emit cameraStarted();
    }
    else
    {
        pimpl->startButton->setEnabled(true);
        pimpl->stopButton->setEnabled(false);
        emit cameraStopped();
    }
}

//--------------------------------------------------------------------------

void QrCodeScanner::updateCameras()
{
    pimpl->deviceList->clear();

    std::vector<QCameraDevice> devices;

    const QList<QCameraDevice> availableCameras = QMediaDevices::videoInputs();
    for (const QCameraDevice &cameraDevice : availableCameras)
    {
        devices.push_back(cameraDevice);
    }

    std::sort(
        devices.begin(),
        devices.end(),
        [](const QCameraDevice &l, const QCameraDevice &r)
        {
            if (l == QMediaDevices::defaultVideoInput())
            {
                return true;
            }
            if (r == QMediaDevices::defaultVideoInput())
            {
                return false;
            }

            return l.description()<r.description();
        }
    );

    for (const auto& device: devices)
    {
        pimpl->deviceList->addItem(device.description(),QVariant::fromValue(device));
    }
}

//--------------------------------------------------------------------------

void QrCodeScanner::displayCameraError()
{
    if (pimpl->camera && pimpl->camera->error() != QCamera::NoError)
    {
        emit displayError(tr("Camera error: %1").arg(pimpl->camera->errorString()));
    }
}

//--------------------------------------------------------------------------

void QrCodeScanner::updateCameraDevice(const QVariant& device)
{
    setCamera(qvariant_cast<QCameraDevice>(device));
}

//--------------------------------------------------------------------------

void QrCodeScanner::onFoundBarcode(const UISE_DESKTOP_NAMESPACE::qrcode::Barcode& barcode)
{
    auto pos=barcode.position();

    if (!pimpl->barcodeFound)
    {
        pimpl->qrcodeFrame->setVisible(true);
    }
    pimpl->barcodeFound=true;

#if 0
qDebug() << "QrCodeScanner::foundBarcode " << QString("Format: \t %1 \nText: \t %2 \nType: \t %3  ").arg(barcode.formatName(),barcode.text(),barcode.contentTypeName())
        << "\t TopLeft:"<<pos.topLeft()
        << "\t TopRight:"<<pos.topRight()
        << "\t BottomLeft:"<<pos.bottomLeft()
        << "\t BottomRight:"<<pos.bottomRight()
        << "\t NativeSize:" << pimpl->videoPreview->nativeSize()
        << "\t Size:" << pimpl->videoPreview->size()
        << "\t Offset:" << pimpl->videoPreview->offset()
        << "\t Current thread:" <<  QThread::currentThread()
        ;
#endif
    QPolygon polygon{
        QList<QPoint>{
            pos.topLeft(),
            pos.topRight(),
            pos.bottomRight(),
            pos.bottomLeft()
        }
    };
    pimpl->qrcodeFrame->setPolygon(polygon);
    emit qrCodeCaptured(barcode);
}

//--------------------------------------------------------------------------

void QrCodeScanner::onMissedBarcode()
{
    if (pimpl->barcodeFound)
    {
        pimpl->barcodeFound=false;
        pimpl->qrcodeFrame->setVisible(false);
    }
}

//--------------------------------------------------------------------------

void QrCodeScanner::setPreviewSize(int width, int height)
{
    if (height<0)
    {
        height=width;
    }
    pimpl->previewSize=QSize(width,height);
    if (pimpl->videoPreview)
    {
        pimpl->videoPreview->setSize(pimpl->previewSize);
    }
}

//--------------------------------------------------------------------------

void QrCodeScanner::setButtonsVisible(bool enable)
{
    pimpl->buttonsFrame->setVisible(enable);
}

//--------------------------------------------------------------------------

bool QrCodeScanner::buttonsVisible() const noexcept
{
    return pimpl->buttonsFrame->isVisible();
}

//--------------------------------------------------------------------------

void QrCodeScanner::onVideoSizeChanged()
{
    auto videoSize=pimpl->videoPreview->videoSink()->videoSize();
    pimpl->videoPreview->setSize(videoSize);

    qreal videoRatio=qreal(videoSize.width())/qreal(videoSize.height());
    auto viewSize=pimpl->previewSize;
    qreal viewRatio=qreal(viewSize.width())/qreal(viewSize.height());
    if (!qFuzzyCompare(videoRatio,viewRatio))
    {
        auto scaleRatio=viewRatio/videoRatio;
        if (scaleRatio>1)
        {
            auto newW=viewSize.width()/scaleRatio;
            viewSize.setWidth(newW);
        }
        else
        {
            auto newH=viewSize.height()*scaleRatio;
            viewSize.setHeight(newH);
        }
    }
    pimpl->view->setFixedSize(viewSize);

    pimpl->view->fitInView(pimpl->videoPreview, Qt::KeepAspectRatio);
    pimpl->deviceList->setFixedSize(pimpl->view->width(),pimpl->deviceList->height());
}

//--------------------------------------------------------------------------

void QrCodeScanner::setQrFrameColor(QColor color)
{
    pimpl->qrFrameColor=color;
    pimpl->qrcodeFrame->setPen(QPen{pimpl->qrFrameColor,pimpl->qrFrameWidth,pimpl->qrFrameStyle});
}

//--------------------------------------------------------------------------

int QrCodeScanner::qrFrameWidth() const noexcept
{
    return static_cast<int>(pimpl->qrFrameWidth);
}

//--------------------------------------------------------------------------

void QrCodeScanner::setQrFrameWidth(int width)
{
    pimpl->qrFrameWidth=width;
    pimpl->qrcodeFrame->setPen(QPen{pimpl->qrFrameColor,pimpl->qrFrameWidth,pimpl->qrFrameStyle});
}

//--------------------------------------------------------------------------

QColor QrCodeScanner::qrFrameColor() const noexcept
{
    return pimpl->qrFrameColor;
}

//--------------------------------------------------------------------------

void QrCodeScanner:: setQrFrameStyle(Qt::PenStyle style)
{
    pimpl->qrFrameStyle=style;
    pimpl->qrcodeFrame->setPen(QPen{pimpl->qrFrameColor,pimpl->qrFrameWidth,pimpl->qrFrameStyle});
}

//--------------------------------------------------------------------------

Qt::PenStyle QrCodeScanner::qrFrameStyle() const noexcept
{
    return pimpl->qrFrameStyle;
}

//--------------------------------------------------------------------------

void QrCodeScanner::resizeEvent(QResizeEvent* event)
{
    QFrame::resizeEvent(event);
    pimpl->deviceList->setFixedSize(pimpl->view->width(),pimpl->deviceList->height());
}

//--------------------------------------------------------------------------

QSize QrCodeScanner::previewSize() const noexcept
{
    return pimpl->previewSize;
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
