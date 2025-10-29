/**
SPDX-License-Identifier: Apache-2.0

Rewritten from zxing-cpp/example/ZXingQtReader.h

See original code and copyright there.

*/

/****************************************************************************/

/** @file uise/desktop/src/qrcodefromimage.cpp
*
*
*/

/****************************************************************************/

#include <uise/desktop/qrcodefromimage.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

namespace qrcode {

/************************** ImageReader ****************************/

//--------------------------------------------------------------------------

QList<Barcode> ImageReader::readBarcodes(const QImage& img, const ReaderOptions& opts)
{
    using namespace ZXing;

    auto ImgFmtFromQImg = [](const QImage& img)
    {
        switch (img.format()) {
            case QImage::Format_ARGB32:
            case QImage::Format_RGB32:
    #if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
                return ImageFormat::BGRA;
    #else
                return ImageFormat::ARGB;
    #endif
            case QImage::Format_RGB888: return ImageFormat::RGB;
            case QImage::Format_RGBX8888:
            case QImage::Format_RGBA8888: return ImageFormat::RGBA;
            case QImage::Format_Grayscale8: return ImageFormat::Lum;
            default: return ImageFormat::None;
        }
    };

    auto exec = [&](const QImage& img)
    {
        return ZXBarcodesToQBarcodes(ZXing::ReadBarcodes(
            {img.bits(), img.width(), img.height(), ImgFmtFromQImg(img), static_cast<int>(img.bytesPerLine())}, opts));
    };

    return ImgFmtFromQImg(img) == ImageFormat::None ? exec(img.convertToFormat(QImage::Format_Grayscale8)) : exec(img);
}

//--------------------------------------------------------------------------

Barcode ImageReader::readBarcode(const QImage& img, const ReaderOptions& opts)
{
    auto res = readBarcodes(img, ReaderOptions(opts).setMaxNumberOfSymbols(1));
    return !res.isEmpty() ? res.takeFirst() : Barcode();
}

//--------------------------------------------------------------------------

} // namespace qrcode
UISE_DESKTOP_NAMESPACE_END
