/**

SPDX-License-Identifier: Apache-2.0

Rewritten from zxing-cpp/example/ZXingQtReader.h

See original code and copyright there.

*/

/****************************************************************************/

/** @file uise/desktop/qrcodefromvideo.hpp
**
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_QRCODE_FROM_VIDEO_HPP
#define UISE_DESKTOP_QRCODE_FROM_VIDEO_HPP

#include <QObject>
#include <QThreadPool>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/qrbarcode.hpp>

class QVideoFrame;
class QVideoSink;

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot expand a macro-opened
// namespace, so it records tr() calls in this file under an unqualified context that does not
// match what moc (a real preprocessor) resolves at runtime -- translations for every string here
// would silently stay in English. Do not revert to the macro form. See task-localization-framework.md.
namespace uise {

namespace qrcode {

class UISE_DESKTOP_EXPORT VideoFrameReader
{
    public:

        static QList<Barcode> readBarcodes(const QVideoFrame& frame, const ReaderOptions& opts = {});

        static Barcode readBarcode(const QVideoFrame& frame, const ReaderOptions& opts = {});
};

class UISE_DESKTOP_EXPORT VideoReader : public QObject,
                                        public ReaderOptions
{
    Q_OBJECT

    public:

        VideoReader(QObject* parent = nullptr);

        ~VideoReader();

        VideoReader(const VideoReader&) = delete;
        VideoReader& operator=(const VideoReader&) = delete;
        VideoReader(VideoReader&&) = delete;
        VideoReader& operator=(VideoReader&&) = delete;

        int formats() const noexcept;

        void setFormats(int newVal);

        TextMode textMode() const noexcept;

        void setTextMode(TextMode newVal);

        Barcode process(const QVideoFrame& image);

        void setVideoSink(QVideoSink* sink);

        void onVideoFrameChanged(const QVideoFrame& frame);

        int maxThreadCount () const;

        void setMaxThreadCount (int maxThreadCount);

    signals:

        void formatsChanged();
        void textModeChanged();
        void failedRead();
        void foundBarcode(const UISE_DESKTOP_NAMESPACE::qrcode::Barcode& barcode);
        void maxThreadCountChanged();

    private:

        QVideoSink* m_sink = nullptr;
        QThreadPool m_pool;
};

} // namespace qrcode

}

#endif // UISE_DESKTOP_QRCODE_FROM_VIDEO_HPP
