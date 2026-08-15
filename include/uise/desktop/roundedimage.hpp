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

/** @file uise/desktop/roundedimagelabel.hpp
*
*  Declares round label widget.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_ROUNDED_IMAGE_HPP
#define UISE_DESKTOP_ROUNDED_IMAGE_HPP

#include <QLabel>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/pixmapproducer.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

class UISE_DESKTOP_EXPORT RoundedImageSource : public PixmapSource
{
    public:

        using PixmapSource::PixmapSource;

        void setXRadius(int value) noexcept
        {
            m_xRadius=value;
        }

        int xRadius() const noexcept
        {
            return m_xRadius.value_or(0);
        }

        void resetXRadius()
        {
            m_xRadius.reset();
        }

        void setYRadius(int value) noexcept
        {
            m_yRadius=value;
        }

        int yRadius() const noexcept
        {
            return m_yRadius.value_or(0);
        }

        void resetYRadius()
        {
            m_yRadius.reset();
        }

        int evalXRadius(int width) const noexcept
        {
            if (!m_xRadius)
            {
                if (m_radiusRatio)
                {
                    return qRound(m_radiusRatio.value()*width);
                }

                return width/2;
            }
            return m_xRadius.value();
        }

        int evalYRadius(int height) const noexcept
        {
            if (!m_yRadius)
            {
                if (m_radiusRatio)
                {
                    return qRound(m_radiusRatio.value()*height);
                }

                return height/2;
            }
            return m_yRadius.value();
        }

        void setRadiusRatio(double ratio) noexcept
        {
            m_radiusRatio=ratio;
        }

        void resetRadiusRatio() noexcept
        {
            m_radiusRatio.reset();
        }

    private:

        std::optional<int> m_xRadius;
        std::optional<int> m_yRadius;

        std::optional<double> m_radiusRatio;
};

//! @todo Rebase RoundedImage on QFrame instead of QLabel.
//!
//! Only two inherited QLabel members are actually used anywhere in the tree: pixmap() (read in
//! paintEvent() as the static-override channel) and setPixmap() (called from
//! fileuploadlistitem.cpp, chatmessagefileitem.cpp, chatmessageimageitem.cpp, imagelabel.cpp,
//! and whitemdesktop's uichatlistitem.cpp). setText()/text() are already =delete'd, paintEvent()
//! is fully overridden and never chains to QLabel::paintEvent(), and nothing uses
//! setMovie/setPicture/setAlignment/setWordWrap/setScaledContents/setBuddy/textInteractionFlags.
//! Replacing the base with QFrame + a plain QPixmap m_pixmap member (setPixmap() calling
//! updateGeometry()+update(), pixmap() returning it) is source-compatible with every call site.
//!
//! This is a hygiene/correctness change, not a performance one -- expect no measurable paint-time
//! win, since paintEvent() already bypasses QLabel's own painting on both bases. The benefits:
//!  - Removes accidental matches from generic "QLabel { ... }"/"QLabel:disabled" QSS rules (see
//!    resources/style/light/reset.qss) that currently apply to every RoundedImage instance
//!    whether intended or not -- worth auditing whitemdesktop/hatnuise QSS for similar
//!    descendant-QLabel selectors that unintentionally reach RoundedImage today.
//!  - Drops one QStyleSheetStyle::subElementRect(SE_LabelLayoutItem) render-rule pass done at
//!    QLabelPrivate::init() time for every instance (a one-time construction cost only).
//! Known behavioural deltas to check before/after:
//!  - sizeHint(): QLabel reports the pixmap's device-independent size (or avgCharWidth x
//!    lineSpacing) expanded to minimumSize(); QFrame reports (-1,-1), falling back to
//!    minimumSize() in layouts. Every current setPixmap() site also calls
//!    setImageSize()/setFixedSize() and every "uise--RoundedImage" QSS rule pins min-*==max-*,
//!    so this is expected to be a no-op, but chatmessagefileitem's preview sizing is worth
//!    double-checking explicitly.
//!  - QMacStyle applies a 1px left layout-item inset for SE_LabelLayoutItem (macOS only); losing
//!    it may nudge icons 1px within their layout cell on macOS.
//!  - Accessibility role drops from Label/Graphic to a generic frame.
//! The real perf lever for widget-heavy screens (e.g. the chat list's ~13 WithRoundedImage
//! instances per row, each a QFrame+QVBoxLayout+RoundedImage) is eliminating that wrapper and
//! painting icons directly, per the flyweight-list painted-elements plan -- not this base-class
//! swap.
class UISE_DESKTOP_EXPORT RoundedImage : public QLabel,
                                         public WithPath
{
    Q_OBJECT

    public:

        explicit RoundedImage(QWidget *parent=nullptr, Qt::WindowFlags f=Qt::WindowFlags());

        void setImageSource(
            std::shared_ptr<RoundedImageSource> source
        );

        void setImageSource(
            std::shared_ptr<RoundedImageSource> source,
            WithPath path,
            const QSize& size={}
        );

        void setImagePath(
            WithPath path
        );

        void setImageSize(
            const QSize& size
        );

        QSize imageSize() const
        {
            return m_size;
        }

        bool isDeviceImageSizeEqual(const QSize& other) const;

        void setAutoSize(bool enable) noexcept
        {
            m_autoSize=enable;
        }

        bool autoSize() const noexcept
        {
            return m_autoSize;
        }

        void setText(const QString&)=delete;
        QString text() const=delete;

        //! @todo Implement configurable circle border

        int xRadius() const;
        int yRadius() const;

        void setCornersRadius(int x, int y)
        {
            m_xRadius=x;
            m_yRadius=y;
            update();
        }

        void resetCornersRadius()
        {
            m_xRadius.reset();
            m_yRadius.reset();
        }

        // note that when SVG icon is used then the corners would not be rounded, it must be done in SVG source
        void setSvgIcon(std::shared_ptr<SvgIcon> svgIcon)
        {
            m_svgIcon=std::move(svgIcon);
            update();
        }

        std::shared_ptr<SvgIcon> svgIcon() const
        {
            return m_svgIcon;
        }

        //! By default the svg icon set via setSvgIcon() is rendered at m_size (the widget's
        //! own image size, see setImageSize()) and painted as a brush filling the whole
        //! rounded rect -- correct for a generic-avatar-style icon meant to fill its shape,
        //! but wrong for a small status/placeholder glyph shown inside a widget sized for
        //! real photo content (e.g. a failed/no-preview chat image tile), where it ends up
        //! stretched to fill the entire (large) tile instead of reading as a small icon.
        //! Passing a valid size here switches to rendering the icon at exactly that size,
        //! centered in the widget's rect via drawPixmap(), independent of m_size/the
        //! widget's own geometry. Pass an invalid QSize (the default) to restore fill mode.
        void setSvgIconSize(const QSize& size)
        {
            m_svgIconSize=size;
            update();
        }

        QSize svgIconSize() const noexcept
        {
            return m_svgIconSize;
        }

        void setParentHovered(bool enable);

        bool isParentHovered() const noexcept
        {
            return m_parentHovered;
        }

        void setSelected(bool enable)
        {
            m_selected=enable;
            update();
        }

        bool isSelected() const noexcept
        {
            return m_selected;
        }

        void setCacheSvgPixmap(bool enable) noexcept
        {
            m_cacheSvgPixmap=enable;
        }

        bool isCacheSvgPixmap() const noexcept
        {
            return m_cacheSvgPixmap;
        }

        void setAutoFitEllipse(bool enable)
        {
            m_autoFitEllipse=enable;
            update();
        }

        bool isAutoFitEllipse() const noexcept
        {
            return m_autoFitEllipse;
        }

        bool isEffectiveHovered() const
        {
            return m_parentHovered || m_hovered;
        }

        IconMode currentSvgIconMode() const;

        void setDisableHover(bool disable)
        {
            m_disableHover=disable;
        }

        bool isHoverDisabled() const noexcept
        {
            return m_disableHover;
        }

    signals:

        void producerDataUpdated(const QVariant& data);

    protected:

        void paintEvent(QPaintEvent *event) override;
        void enterEvent(QEnterEvent* event) override;
        void leaveEvent(QEvent* event) override;
        void changeEvent(QEvent* event) override;

        virtual void doPaint(QPainter*)
        {}

        virtual void fillIfNoPixmap(QPainter* painter)
        {
            std::ignore=painter;
        }

    private slots:

        void onPixmapUpdated();

    private:

        void createPixmapConsumer();

        PixmapConsumer* m_pixmapConsumer;
        PixmapConsumer* m_prevPixmapConsumer;
        std::shared_ptr<RoundedImageSource> m_imageSource;
        QSize m_size;

        std::optional<int> m_xRadius;
        std::optional<int> m_yRadius;

        bool m_autoSize;

        std::shared_ptr<SvgIcon> m_svgIcon;
        QSize m_svgIconSize;
        bool m_hovered;
        bool m_parentHovered;
        bool m_selected;
        bool m_cacheSvgPixmap;
        bool m_autoFitEllipse;

        bool m_disableHover;
};

class UISE_DESKTOP_EXPORT WithRoundedImage : public QFrame
{
    Q_OBJECT

    public:

        explicit WithRoundedImage(QWidget *parent=nullptr);

        RoundedImage* image() const noexcept
        {
            return m_img;
        }

    private:

        RoundedImage* m_img;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_ROUNDED_IMAGE_HPP
