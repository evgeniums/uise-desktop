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
#include <uise/desktop/imageanimator.hpp>

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot expand a macro-opened
// namespace, so it records tr() calls in this file under an unqualified context that does not
// match what moc (a real preprocessor) resolves at runtime -- translations for every string here
// would silently stay in English. Do not revert to the macro form. See task-localization-framework.md.
namespace uise {

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

            //! Seed encoded animation content -- resolved the same way content is (either this
            //! seed, or -- when null and setImageSource() is set -- live through the source, see
            //! PixmapSource::updateAnimation()/updatePathAnimation()). Whether a resolved preview
            //! actually animates additionally depends on setAnimationMode() (default Never: a
            //! preview holding animation content still shows only its first frame, matching
            //! Telegram's own album-strip behaviour, unless the host opts in).
            AnimationContent animation;

            Preview(PixmapKey key={}, QPixmap content={}, AnimationContent animation={})
                : key(std::move(key)), content(std::move(content)), animation(std::move(animation))
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

        /**
         * @brief Replace the set of previews shown, diffing against the previous set.
         * @param previews New previews, in display order.
         * @param currentIndex Index (into previews) to select.
         *
         * A key already present in the previous set keeps its widget and, if already resolved, its
         * live PixmapConsumer -- so a mostly-overlapping replacement (e.g. a continuous strip
         * sliding by one image as the viewer's window moves) does not re-fetch or re-flash
         * thumbnails that were already showing correctly. Keys not present in the new set are
         * destroyed; keys not present in the previous set are created fresh.
         *
         * The transition animates (see setScrollAnimationDurationMs()) when the previously-current
         * preview's key is still present somewhere in the new set -- i.e. the strip is still
         * looking at "the same neighbourhood", just re-centred -- and jumps with no animation
         * otherwise, which is the common case for a disjoint album-to-album swap.
         */
        void setPreviews(std::vector<Preview> previews, int currentIndex=0);

        //! Equivalent to setPreviews({}).
        void clear();

        int count() const noexcept;
        int currentIndex() const noexcept;

        //! Window-relative index of key, or -1 if key is not currently shown. Re-resolve after
        //! any setPreviews() call rather than caching -- diffing can shift every index.
        int indexOf(const UISE_DESKTOP_NAMESPACE::PixmapKey& key) const;

        //! Equivalent to setCurrentIndex(indexOf(key)); no-op if key is not currently shown.
        void setCurrentPreview(const UISE_DESKTOP_NAMESPACE::PixmapKey& key);

        /**
         * @brief Set the source used to resolve previews whose Preview::content was left null.
         * @param source New source, replacing any previously set. Previews supplied with
         *  non-null content are unaffected either way.
         *
         * Previews already resolved through a DIFFERENT prior source are re-wired onto this one
         * (its producer released, this source's acquired) the next time their content is
         * (re-)applied; previews already resolved through the SAME source (i.e. this call is a
         * no-op change) are left exactly as they are.
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

        /**
         * @brief Set whether items with animation content actually play it, default Never.
         *
         * Telegram's own album strip keeps thumbnails still even when the full image is animated
         * -- Never matches that and costs nothing extra (each item is already an ImageLabel, see
         * the class doc, just never handed animation content). Auto/OnHover/Manual opt in at the
         * cost of one concurrent QMovie decoder per visible animated item.
         */
        void setAnimationMode(UISE_DESKTOP_NAMESPACE::ImageAnimator::AnimationMode mode);
        UISE_DESKTOP_NAMESPACE::ImageAnimator::AnimationMode animationMode() const;

        QSize sizeHint() const override;

    public slots:

        //! Animate to a new current index; clamped to [0,count()-1]. No-op when empty.
        void setCurrentIndex(int index);

    signals:

        //! Emitted when a preview other than a plain drag is clicked. index is resolved at click
        //! time (see previewClickedKey()) so it is correct even if setPreviews() reordered items
        //! since the click target was last wired.
        void previewClicked(int index);

        //! Same event as previewClicked(), identifying the preview by key -- the form to prefer
        //! when the receiver will turn around and call something key-addressed (e.g.
        //! AbstractImageViewer::selectImage(const PixmapKey&)) rather than an index.
        void previewClickedKey(const UISE_DESKTOP_NAMESPACE::PixmapKey& key);

    protected:

        void resizeEvent(QResizeEvent* event) override;

    private:

        void relayout();

        //! Diffs pimpl->previews against the current pimpl->items: keys present in both keep
        //! their widget/consumer, keys only in the old set are destroyed, keys only in the new set
        //! are created. Replaces the old destroy-everything-and-rebuild rebuildItems().
        void diffItems();

        void applyItemContent(size_t index);

        //! Seed-or-source-resolved counterpart of applyItemContent(), for Preview::animation --
        //! the source-resolved case piggybacks on the same consumer wireItemConsumer() wires (it
        //! connects both PixmapConsumer::pixmapUpdated and ::animationUpdated together), so this
        //! only has direct work to do for the seed/no-source cases.
        void applyItemAnimation(size_t index);

        //! Connect an entry's consumer to pimpl->imageSource for the first time, or re-point it
        //! at a source that changed since it was last wired (see setImageSource()'s doc on why
        //! that case needs handling explicitly).
        void wireItemConsumer(size_t index);

        void setItemPixmap(size_t index, const QPixmap& px);

        //! No-op if animation equals the entry's last-applied animation content (ItemEntry::
        //! appliedAnimation) -- guards against restarting playback on every redundant call (e.g.
        //! a diffItems() pass for an item whose animation content did not actually change).
        void setItemAnimation(size_t index, const UISE_DESKTOP_NAMESPACE::AnimationContent& animation);

        void applyItemOpacity(size_t index);

        std::unique_ptr<ImagePreviewStrip_p> pimpl;
};

}

#endif // UISE_DESKTOP_IMAGEPREVIEWSTRIP_HPP
