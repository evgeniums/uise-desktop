/**
@copyright Evgeny Sidorov 2021

This software is dual-licensed. Choose the appropriate license for your project.

1. The GNU GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-GPLv3.md](LICENSE-GPLv3.md) or copy at https://www.gnu.org/licenses/gpl-3.0.txt)
    
2. The GNU LESSER GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-LGPLv3.md](LICENSE-LGPLv3.md) or copy at https://www.gnu.org/licenses/lgpl-3.0.txt).

You may select, at your option, one of the above-listed licenses.

*/

/****************************************************************************/

/** @file uise/desktop/qrcodescanner.hpp
**
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_QRCODE_SCANNER_HPP
#define UISE_DESKTOP_QRCODE_SCANNER_HPP

#include <memory>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/widget.hpp>

class QCameraDevice;

UISE_DESKTOP_NAMESPACE_BEGIN

namespace qrcode
{
class Barcode;
}

class QrCodeScanner_p;

class UISE_DESKTOP_EXPORT QrCodeScanner : public WidgetQFrame
{
    Q_OBJECT

    public:

        QrCodeScanner(QWidget* parent=nullptr);

        ~QrCodeScanner();

        QrCodeScanner(const QrCodeScanner&)=delete;
        QrCodeScanner(QrCodeScanner&&)=delete;
        QrCodeScanner& operator=(const QrCodeScanner&)=delete;
        QrCodeScanner& operator=(QrCodeScanner&&)=delete;

        virtual void construct() override;

        QWidget* qWidget() override
        {
            return this;
        }

        void init();

    public slots:

        void start();
        void stop();

    signals:

        void qrCodeCaptured();
        void displayError(const QString& error);

    private slots:

        void onFoundBarcode(const UISE_DESKTOP_NAMESPACE::qrcode::Barcode& barcode);

    private:

        void setCamera(const QCameraDevice &cameraDevice);
        void updateCameraActive(bool active);
        void updateCameras();
        void displayCameraError();
        void updateCameraDevice(const QVariant& device);

        std::unique_ptr<QrCodeScanner_p> pimpl;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_QRCODE_SCANNER_HPP
