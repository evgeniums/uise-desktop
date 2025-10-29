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
#include <QImageCapture>
#include <QMediaCaptureSession>
#include <QtMultimediaWidgets/QVideoWidget>
#include <QMessageBox>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/utils/destroywidget.hpp>
#include <uise/desktop/style.hpp>
#include <uise/desktop/pushbutton.hpp>
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

        QFrame* videoFrame;
        QVideoWidget *videoPreview;

        QFrame* buttonsFrame;
        PushButton* startButton;
        PushButton* stopButton;

        qrcode::VideoReader* qrCodeReader;

        bool initialized=false;
};

//--------------------------------------------------------------------------

QrCodeScanner::QrCodeScanner(QWidget* parent)
    : WidgetQFrame(parent),
      pimpl(std::make_unique<QrCodeScanner_p>())
{
}

//--------------------------------------------------------------------------

void QrCodeScanner::construct()
{
    auto l=Layout::vertical(this);

    pimpl->videoFrame=new QFrame(this);
    pimpl->videoFrame->setObjectName("videoFrame");
    auto vl=Layout::horizontal(pimpl->videoFrame);
    l->addWidget(pimpl->videoFrame,0,Qt::AlignHCenter);
    pimpl->videoPreview=new QVideoWidget(pimpl->videoFrame);
    pimpl->videoPreview->setFixedSize(640,640);
    vl->addWidget(pimpl->videoPreview,0,Qt::AlignCenter);

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
    QRect r{pos.topLeft(),pos.bottomRight()};

qDebug() << "QrCodeScanner::foundBarcode " << QString("Format: \t %1 \nText: \t %2 \nType: \t %3  ").arg(barcode.formatName(),barcode.text(),barcode.contentTypeName()) << "\n Size:" << r.size()
        << "\t TopLeft:"<<pos.topLeft()
        << "\t TopRight:"<<pos.topRight()
        << "\t BottomLeft:"<<pos.bottomLeft()
        << "\t BottomRight:"<<pos.bottomRight()
        ;
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
