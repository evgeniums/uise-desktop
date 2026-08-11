/**
@copyright Evgeny Sidorov 2026

This software is dual-licensed. Choose the appropriate license for your project.

1. The GNU GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-GPLv3.md](LICENSE-GPLv3.md) or copy at https://www.gnu.org/licenses/gpl-3.0.txt)

2. The GNU LESSER GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-LGPLv3.md](LICENSE-LGPLv3.md) or copy at https://www.gnu.org/licenses/lgpl-3.0.txt).

You may select, at your option, one of the above-listed licenses.

*/

/****************************************************************************/

/** @file uise/desktop/imagepreviewstrip.hpp
*
*  Declares ImagePreviewStrip.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_IMAGEPREVIEWSTRIP_HPP
#define UISE_DESKTOP_IMAGEPREVIEWSTRIP_HPP

#include <memory>
#include <vector>

#include <QFrame>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/pixmapproducer.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

class ImagePreviewStrip_p;

/**
 * @brief Horizontal strip of small clickable image previews, centred on a current index with
 *  neighbouring previews fading out with distance -- the "images slider" of a Telegram-style
 *  chat image viewer (see ChatImageViewerControls), but deliberately generic and not itself
 *  chat-specific, same as e.g. FileDropOverlay is a generic building block rather than a
 *  chat-only widget.
 *
 * Content for each preview is resolved the same way AbstractImageViewer resolves its own images:
 * either supplied directly via Preview::content, or, when content is null and setImageSource()
 * has been called, fetched through a PixmapConsumer bound to that PixmapSource and updated live
 * as the producer's pixmap improves.
 *
 * Shows nothing (no thumbnails, no interaction) whenever count()<=1 -- a single-image message
 * has no album to browse. Deliberately does NOT hide the widget itself via setVisible(false):
 * this widget always keeps its own layout slot (constant sizeHint(), independent of item count),
 * so a host embedding it in a row alongside fixed-size siblings (see ChatImageViewerControls)
 * gets a stable width/height regardless of whether an album happens to be showing.
 */
class UISE_DESKTOP_EXPORT ImagePreviewStrip : public QFrame
{
    Q_OBJECT

    Q_PROPERTY(QSize itemSize READ itemSize WRITE setItemSize)
    Q_PROPERTY(int spacing READ spacing WRITE setSpacing)
    Q_PROPERTY(qreal minOpacity READ minOpacity WRITE setMinOpacity)
    Q_PROPERTY(int visibleNeighbours READ visibleNeighbours WRITE setVisibleNeighbours)
    Q_PROPERTY(int scrollAnimationDurationMs READ scrollAnimationDurationMs WRITE setScrollAnimationDurationMs)

    //! Animated float position of the centred item. Not meant to be set directly by client code.
    Q_PROPERTY(qreal visualIndex READ visualIndex WRITE setVisualIndex)

    public:

        //! One entry of the strip, mirroring AbstractImageViewer::Image's key+content shape.
        struct Preview
        {
            PixmapKey key;
            QPixmap content;

            Preview(PixmapKey key={}, QPixmap content={}) : key(std::move(key)), content(std::move(content))
            {}
        };

        constexpr static const int DefaultItemSize=44;
        constexpr static const int DefaultSpacing=6;
        constexpr static const qreal DefaultMinOpacity=0.25;
        constexpr static const int DefaultVisibleNeighbours=3;
        constexpr static const int DefaultScrollAnimationDurationMs=200;

        explicit ImagePreviewStrip(QWidget* parent=nullptr);

        ~ImagePreviewStrip();
        ImagePreviewStrip(const ImagePreviewStrip&)=delete;
        ImagePreviewStrip(ImagePreviewStrip&&)=delete;
        ImagePreviewStrip& operator=(const ImagePreviewStrip&)=delete;
        ImagePreviewStrip& operator=(ImagePreviewStrip&&)=delete;

        //! Replace the whole set of previews and jump (no animation) to currentIndex.
        void setPreviews(std::vector<Preview> previews, int currentIndex=0);

        //! Equivalent to setPreviews({}).
        void clear();

        int count() const noexcept;
        int currentIndex() const noexcept;

        /**
         * @brief Set the source used to resolve previews whose Preview::content was left null.
         * @param source New source. Only affects previews resolved after this call -- previews
         *  supplied with non-null content, or already resolved via a previously-set source, are
         *  unaffected by calling this again.
         */
        void setImageSource(std::shared_ptr<PixmapSource> source);
        std::shared_ptr<PixmapSource> imageSource() const;

        QSize itemSize() const;
        void setItemSize(QSize size);

        int spacing() const;
        void setSpacing(int px);

        //! Opacity floor for the outermost neighbours; the centred item is always fully opaque.
        qreal minOpacity() const;
        void setMinOpacity(qreal value);

        //! Distance, in items, at which a neighbour's opacity reaches minOpacity().
        int visibleNeighbours() const;
        void setVisibleNeighbours(int n);

        int scrollAnimationDurationMs() const;
        void setScrollAnimationDurationMs(int ms);

        qreal visualIndex() const;
        void setVisualIndex(qreal value);

        QSize sizeHint() const override;

    public slots:

        //! Animate to a new current index; clamped to [0,count()-1]. No-op when empty.
        void setCurrentIndex(int index);

    signals:

        //! Emitted when a preview other than a plain drag is clicked.
        void previewClicked(int index);

    protected:

        void resizeEvent(QResizeEvent* event) override;

    private:

        void relayout();
        void rebuildItems();
        void applyItemContent(size_t index);
        void setItemPixmap(size_t index, const QPixmap& px);
        void applyItemOpacity(size_t index);

        std::unique_ptr<ImagePreviewStrip_p> pimpl;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_IMAGEPREVIEWSTRIP_HPP
