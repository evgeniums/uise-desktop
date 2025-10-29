/**
SPDX-License-Identifier: Apache-2.0

Rewritten from zxing-cpp/example/ZXingQtReader.h

See original code and copyright there.

*/

/****************************************************************************/

/** @file uise/desktop/qrcodefromimage.hpp
**
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_QRCODE_FROM_IMAGE_HPP
#define UISE_DESKTOP_QRCODE_FROM_IMAGE_HPP

#include <QImage>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/qrbarcode.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

namespace qrcode {

class UISE_DESKTOP_EXPORT ImageReader
{
    public:

        static QList<Barcode> readBarcodes(const QImage& img, const ReaderOptions& opts = {});

        static Barcode readBarcode(const QImage& img, const ReaderOptions& opts = {});
};

} // namespace qrcode

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_QRCODE_FROM_IMAGE_HPP
