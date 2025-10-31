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
#include <QMessageBox>
#include <QThread>

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
};

//--------------------------------------------------------------------------

QrCodeScanner::QrCodeScanner(QWidget* parent)
    : WidgetQFrame(parent),
      pimpl(std::make_unique<QrCodeScanner_p>())
{
    pimpl->previewSize=QSize(DefaultPreviewSize,DefaultPreviewSize);
}

//--------------------------------------------------------------------------

void QrCodeScanner::construct()
{
    auto l=Layout::vertical(this);

    l->addStretch(1);

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
    pimpl->camera.reset(new QCamera(cameraDevice));
    pimpl->captureSession.setCamera(pimpl->camera.get());

    connect(pimpl->camera.get(), &QCamera::activeChanged, this, &QrCodeScanner::updateCameraActive);
    connect(pimpl->camera.get(), &QCamera::errorOccurred, this, &QrCodeScanner::displayCameraError);

    pimpl->captureSession.setVideoOutput(pimpl->videoPreview);

    updateCameraActive(pimpl->camera->isActive());
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
    if (active)
    {
        pimpl->startButton->setEnabled(false);
        pimpl->stopButton->setEnabled(true);        
    }
    else
    {
        pimpl->startButton->setEnabled(true);
        pimpl->stopButton->setEnabled(false);
    }
}

//--------------------------------------------------------------------------

void QrCodeScanner::updateCameras()
{
    // ui->menuDevices->clear();
    // const QList<QCameraDevice> availableCameras = QMediaDevices::videoInputs();
    // for (const QCameraDevice &cameraDevice : availableCameras) {
    //     QAction *videoDeviceAction = new QAction(cameraDevice.description(), videoDevicesGroup);
    //     videoDeviceAction->setCheckable(true);
    //     videoDeviceAction->setData(QVariant::fromValue(cameraDevice));
    //     if (cameraDevice == QMediaDevices::defaultVideoInput())
    //         videoDeviceAction->setChecked(true);

    //     ui->menuDevices->addAction(videoDeviceAction);
    // }
}

//--------------------------------------------------------------------------

void QrCodeScanner::displayCameraError()
{
    if (pimpl->camera && pimpl->camera->error() != QCamera::NoError)
    {
        //! @todo Show error
        QMessageBox::warning(this, tr("Camera Error"), pimpl->camera->errorString());
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
    auto viewSize=pimpl->view->size();
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
        pimpl->view->setFixedSize(viewSize);
    }

    pimpl->view->fitInView(pimpl->videoPreview, Qt::KeepAspectRatio);
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

UISE_DESKTOP_NAMESPACE_END
