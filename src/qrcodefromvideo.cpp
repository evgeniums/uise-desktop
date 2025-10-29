/**

SPDX-License-Identifier: Apache-2.0

Rewritten from zxing-cpp/example/ZXingQtReader.h

See original code and copyright there.

*/

/****************************************************************************/

/** @file uise/desktop/src/qrcodefromvideo.cpp
*
*/

/****************************************************************************/

#include <QVideoFrame>
#include <QVideoSink>
#include <QDebug>

#include <uise/desktop/qrcodefromimage.hpp>
#include <uise/desktop/qrcodefromvideo.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

namespace qrcode {

/************************** VideoFrameReader ****************************/

//--------------------------------------------------------------------------

QList<Barcode> VideoFrameReader::readBarcodes(const QVideoFrame& frame, const ReaderOptions& opts)
{
    using namespace ZXing;

    ImageFormat fmt = ImageFormat::None;
    int pixStride = 0;
    int pixOffset = 0;

    #define FORMAT(F5, F6) QVideoFrameFormat::Format_##F6
    #define FIRST_PLANE 0

    switch (frame.pixelFormat()) {
    case FORMAT(ARGB32, ARGB8888):
    case FORMAT(ARGB32_Premultiplied, ARGB8888_Premultiplied):
    case FORMAT(RGB32, RGBX8888):
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
        fmt = ImageFormat::BGRA;
#else
        fmt = ImageFormat::ARGB;
#endif
        break;

    case FORMAT(BGRA32, BGRA8888):
    case FORMAT(BGRA32_Premultiplied, BGRA8888_Premultiplied):
    case FORMAT(BGR32, BGRX8888):
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
        fmt = ImageFormat::RGBA;
#else
        fmt = ImageFormat::ABGR;
#endif
        break;

    case QVideoFrameFormat::Format_P010:
    case QVideoFrameFormat::Format_P016: fmt = ImageFormat::Lum, pixStride = 1; break;

    case FORMAT(AYUV444, AYUV):
    case FORMAT(AYUV444_Premultiplied, AYUV_Premultiplied):
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
        fmt = ImageFormat::Lum, pixStride = 4, pixOffset = 3;
#else
        fmt = ImageFormat::Lum, pixStride = 4, pixOffset = 2;
#endif
        break;

    case FORMAT(YUV420P, YUV420P):
    case FORMAT(NV12, NV12):
    case FORMAT(NV21, NV21):
    case FORMAT(IMC1, IMC1):
    case FORMAT(IMC2, IMC2):
    case FORMAT(IMC3, IMC3):
    case FORMAT(IMC4, IMC4):
    case FORMAT(YV12, YV12): fmt = ImageFormat::Lum; break;
    case FORMAT(UYVY, UYVY): fmt = ImageFormat::Lum, pixStride = 2, pixOffset = 1; break;
    case FORMAT(YUYV, YUYV): fmt = ImageFormat::Lum, pixStride = 2; break;

    case FORMAT(Y8, Y8): fmt = ImageFormat::Lum; break;
    case FORMAT(Y16, Y16): fmt = ImageFormat::Lum, pixStride = 2, pixOffset = 1; break;

    case FORMAT(ABGR32, ABGR8888):
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
        fmt = ImageFormat::RGBA;
#else
        fmt = ImageFormat::ABGR;
#endif
        break;
    case FORMAT(YUV422P, YUV422P): fmt = ImageFormat::Lum; break;
    default: break;
    }

    if (fmt != ImageFormat::None) {
        auto img = frame; // shallow copy just get access to non-const map() function
        if (!img.isValid() || !img.map(QVideoFrame::ReadOnly))
        {
            qWarning() << "invalid QVideoFrame: could not map memory";
            return {};
        }
        QScopeGuard unmap([&] { img.unmap(); });

        return ZXBarcodesToQBarcodes(ZXing::ReadBarcodes(
            {img.bits(FIRST_PLANE) + pixOffset, img.width(), img.height(), fmt, img.bytesPerLine(FIRST_PLANE), pixStride}, opts));
    }
    else {
        auto qimg = frame.toImage();
        if (qimg.format() != QImage::Format_Invalid)
        {
            return ImageReader::readBarcodes(qimg, opts);
        }
        qWarning() << "failed to convert QVideoFrame to QImage";
        return {};
    }
}

//--------------------------------------------------------------------------

Barcode VideoFrameReader::readBarcode(const QVideoFrame& frame, const ReaderOptions& opts)
{
    auto res = readBarcodes(frame, ReaderOptions(opts).setMaxNumberOfSymbols(1));
    return !res.isEmpty() ? res.takeFirst() : Barcode();
}

/************************** VideoFrameReader ****************************/

//--------------------------------------------------------------------------

VideoReader::VideoReader(QObject* parent) : QObject(parent)
{
    m_pool.setMaxThreadCount(1);
}

//--------------------------------------------------------------------------

VideoReader::~VideoReader()
{
    m_pool.setMaxThreadCount(0);
    m_pool.waitForDone(-1);
}

//--------------------------------------------------------------------------

int VideoReader::formats() const noexcept
{
    auto fmts = ReaderOptions::formats();
    return *reinterpret_cast<int*>(&fmts);
}

//--------------------------------------------------------------------------

void VideoReader::setFormats(int newVal)
{
    if (formats() != newVal)
    {
        ReaderOptions::setFormats(static_cast<ZXing::BarcodeFormat>(newVal));
        emit formatsChanged();
    }
}

//--------------------------------------------------------------------------

TextMode VideoReader::textMode() const noexcept
{
    return static_cast<TextMode>(ReaderOptions::textMode());
}

//--------------------------------------------------------------------------

void VideoReader::setTextMode(TextMode newVal)
{
    if (textMode() != newVal)
    {
        ReaderOptions::setTextMode(static_cast<ZXing::TextMode>(newVal));
        Q_EMIT textModeChanged();
    }
}

//--------------------------------------------------------------------------

Barcode VideoReader::process(const QVideoFrame& image)
{
    // QElapsedTimer t;
    // t.start();

    auto res = VideoFrameReader::readBarcode(image, *this);

    // runTime = t.elapsed();

    if (res.isValid())
    {
        emit foundBarcode(res);
    }
    else
    {
        emit failedRead();
    }
    return res;
}

//--------------------------------------------------------------------------

void VideoReader::setVideoSink(QVideoSink* sink)
{
    if (m_sink == sink)
    {
        return;
    }

    if (m_sink)
    {
        disconnect(m_sink, nullptr, this, nullptr);
    }

    m_sink = sink;
    connect(
        m_sink,
        &QVideoSink::videoFrameChanged,
        this,
        &VideoReader::onVideoFrameChanged,
        Qt::DirectConnection
    );
}

//--------------------------------------------------------------------------

void VideoReader::onVideoFrameChanged(const QVideoFrame& frame)
{
    if (m_pool.activeThreadCount() >= m_pool.maxThreadCount())
    {
        return; // we are busy => skip the frame
    }

    m_pool.start(
        [this, frame]()
        {
            process(frame);
        }
    );
}

//--------------------------------------------------------------------------

int VideoReader::maxThreadCount () const
{
    return m_pool.maxThreadCount();
}

//--------------------------------------------------------------------------

void VideoReader::setMaxThreadCount (int maxThreadCount)
{
    if (m_pool.maxThreadCount() != maxThreadCount)
    {
        m_pool.setMaxThreadCount(maxThreadCount);
        emit maxThreadCountChanged();
    }
}

//--------------------------------------------------------------------------

} // namespace qrcode
UISE_DESKTOP_NAMESPACE_END
