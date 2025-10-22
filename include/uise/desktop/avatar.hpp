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

/** @file uise/desktop/avatar.hpp
*
*  Declares avatar image source.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_AVATAR_HPP
#define UISE_DESKTOP_AVATAR_HPP

#include <QColor>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/svgicon.hpp>
#include <uise/desktop/roundedimage.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

class AvatarSource;

class UISE_DESKTOP_EXPORT AvatarBackgroundGenerator
{
    public:

        constexpr static const QRgb DefaultBackgroundColor=0x00669bbc;

        AvatarBackgroundGenerator(std::vector<QColor> backgroundPallette={});
        virtual ~AvatarBackgroundGenerator();

        AvatarBackgroundGenerator(const AvatarBackgroundGenerator&)=default;
        AvatarBackgroundGenerator(AvatarBackgroundGenerator&&)=default;
        AvatarBackgroundGenerator& operator=(const AvatarBackgroundGenerator&)=default;
        AvatarBackgroundGenerator& operator=(AvatarBackgroundGenerator&&)=default;

        void setBackgroundPallette(std::vector<QColor> pallette)
        {
            m_backgroundPallette=std::move(pallette);
        }

        const std::vector<QColor>& backgroundPallette() const
        {
            return m_backgroundPallette;
        }

        virtual QColor generateBackgroundColor(const WithPath& path) const;

    private:

        std::vector<QColor> m_backgroundPallette;
};

class UISE_DESKTOP_EXPORT Avatar : public WithPath
{
    public:

        constexpr static const double DefaultFontSizeRatio=0.37;
        constexpr static QRgb DefaultFontColor=0x00FFFFFF;

        Avatar();

        virtual ~Avatar();

        Avatar(const Avatar&)=delete;
        Avatar(Avatar&&)=delete;
        Avatar& operator=(const Avatar&)=delete;
        Avatar& operator=(Avatar&&)=delete;

        void setAvatarName(std::string name)
        {
            m_avatarName=std::move(name);
            updateGeneratedAvatar();
        }

        std::string avatarName() const
        {
            return m_avatarName;
        }

        template <typename PathT>
        void setAvatarPath(PathT path)
        {
            setPath(std::move(path));
            updateAvatarPath();
            updateBackgroundColor();
            updateGeneratedAvatar();
        }

        const auto& avatarPath() const noexcept
        {
            return path();
        }

        void setBasePixmap(const QPixmap& pixmap)
        {
            m_basePixmap=pixmap;
            updateProducers();
        }

        QPixmap basePixmap() const
        {
            return m_basePixmap;
        }

        void setPixmap(const QPixmap& pixmap);

        QPixmap pixmap(const QSize& size) const;

        void setAvatarSource(AvatarSource* source) noexcept
        {
            m_avatarSource=source;
            updateBackgroundColor();
            updateGeneratedAvatar();
        }

        AvatarSource* avatarSource() const noexcept
        {
            return m_avatarSource;
        }

        QColor fontColor() const;

        void setFontColor(QColor color)
        {
            m_fontColor=color;
        }

        void resetFontColor()
        {
            m_fontColor.reset();
        }

        double fontSizeRatio() const;

        void setFontSizeRatio(double ratio)
        {
            m_fontSizeRatio=ratio;
        }

        void setFontSizeRatio()
        {
            m_fontSizeRatio.reset();
        }

        void setBackgroundColorGenerator(std::shared_ptr<AvatarBackgroundGenerator> backgroundColorGenerator)
        {
            m_backgroundColorGenerator=std::move(backgroundColorGenerator);
        }

        std::shared_ptr<AvatarBackgroundGenerator> backgroundColorGenerator() const;

        QPixmap generatePixmap(const QSize& size) const;

        const std::set<QSize,compareQSize> watchPixmapSizes() const
        {
            return m_watchSizes;
        }

        void watchPixmapForSize(const QSize& size)
        {
            m_watchSizes.insert(size);
            doWatchPixmapForSize(size);
        }

        void unwatchPixmapForSize(const QSize& size)
        {
            m_watchSizes.erase(size);
            doUnwatchPixmapForSize(size);
        }

    protected:

        void updateProducers();

        virtual void doWatchPixmapForSize(const QSize&)
        {
        }

        virtual void doUnwatchPixmapForSize(const QSize&)
        {
        }

        virtual void updateAvatarPath()
        {}

    private:

        void updateGeneratedAvatar();
        void updateBackgroundColor();

        std::string m_avatarName;

        QPixmap m_basePixmap;

        AvatarSource* m_avatarSource;

        QColor m_backgroundColor;
        std::optional<QColor> m_fontColor;
        std::optional<double> m_fontSizeRatio;

        std::shared_ptr<AvatarBackgroundGenerator> m_backgroundColorGenerator;

        std::map<QSize,QPixmap,compareQSize> m_pixmaps;
        std::set<QSize,compareQSize> m_watchSizes;
};

class UISE_DESKTOP_EXPORT AvatarSource : public RoundedImageSource
{
    public:

        constexpr static const char* DefaultFontName="Helvetica";
        constexpr static const size_t DefaultMaxAvatarLetterCount=2;        

        using AvatarBuilderFn=std::function<std::shared_ptr<Avatar> (const WithPath&)>;

        AvatarSource();

        //! @todo Configure font and pallette from Style

        void setFontName(QString name)
        {
            m_fontName=std::move(name);
        }

        QString fontName() const
        {
            return m_fontName;
        }

        void setFontColor(const QColor& color)
        {
            m_fontColor=color;
        }

        QColor fontColor() const noexcept
        {
            return m_fontColor;
        }

        double fontSizeRatio() const
        {
            //! @todo Use from avatar source
            return m_fontSizeRatio;
        }

        void setFontSizeRatio(double ratio)
        {
            m_fontSizeRatio=ratio;
        }

        void setMaxAvatarLetterCount(size_t count)
        {
            m_maxAvatarLetterCount=count;
        }

        int maxAvatarLetterCount() const noexcept
        {
            return m_maxAvatarLetterCount;
        }

        void clearAvatars();

        void setBackgroundPallette(std::vector<QColor> pallette)
        {
            m_backgroundPallette=std::move(pallette);
        }

        const std::vector<QColor>& backgroundPallette() const
        {
            return m_backgroundPallette;
        }

        std::shared_ptr<Avatar> avatar(const WithPath& path) const
        {
            auto it=m_avatars.find(path);
            if (it!=m_avatars.end())
            {
                return it->second;
            }
            return std::shared_ptr<Avatar>{};
        }

        void setAvatarBuilder(AvatarBuilderFn avatarBuilder)
        {
            m_avatarBuilder=avatarBuilder;
        }

        AvatarBuilderFn avatarBuilder() const
        {
            return m_avatarBuilder;
        }

        void setNoNameSvgIcon(std::shared_ptr<SvgIcon> icon)
        {
            m_noNameSvgIcon=std::move(icon);
        }

        std::shared_ptr<SvgIcon> noNameSvgIcon() const
        {
            return m_noNameSvgIcon;
        }

        void setBackgroundColorGenerator(std::shared_ptr<AvatarBackgroundGenerator> backgroundColorGenerator)
        {
            m_backgroundColorGenerator=std::move(backgroundColorGenerator);
        }

        std::shared_ptr<AvatarBackgroundGenerator> backgroundColorGenerator() const
        {
            return m_backgroundColorGenerator;
        }

    protected:

        void doLoadProducer(const PixmapKey& key) override;

        void doUnloadProducer(const PixmapKey& key) override;

        void doLoadPixmap(const PixmapKey& key) override;

    private:

        std::map<WithPath,std::shared_ptr<Avatar>> m_avatars;

        QString m_fontName;
        QColor m_fontColor;
        double m_fontSizeRatio;
        size_t m_maxAvatarLetterCount;
        std::vector<QColor> m_backgroundPallette;
        std::shared_ptr<SvgIcon> m_noNameSvgIcon;

        std::shared_ptr<AvatarBackgroundGenerator> m_backgroundColorGenerator;
        AvatarBuilderFn m_avatarBuilder;
};

class UISE_DESKTOP_EXPORT AvatarWidget : public RoundedImage
{
    Q_OBJECT

    public:

        constexpr static const double DefaultFontSizeRation=0.37;
        constexpr static const double CornerImageSizeRatio=0.2;
        constexpr static QRgb DefaultCornerCircleColor=0x0038b000;
        constexpr static QRgb DefaultFontColor=0x00FFFFFF;

        using RoundedImage::RoundedImage;

        void setRightBottomCircle(bool enable) noexcept
        {
            m_rightBottomCircle=enable;
            update();
        }

        bool isRightBottomCircle() const noexcept
        {
            return m_rightBottomCircle;
        }

        void setRightBottomPixmap(QPixmap pixmap)
        {
            m_rightBottomPixmap=std::move(pixmap);
            update();
        }

        QPixmap rightBottomWidget() const
        {
            return m_rightBottomPixmap;
        }

        void setRightBottomSvgIcon(std::shared_ptr<SvgIcon> icon)
        {
            m_rightBottomSvgIcon=std::move(icon);
            update();
        }

        std::shared_ptr<SvgIcon> rightBottomSvgIcon() const
        {
            return m_rightBottomSvgIcon;
        }

        void clearRightBottomCorner()
        {
            m_rightBottomCircle=false;
            m_rightBottomSvgIcon.reset();
            m_rightBottomPixmap=QPixmap{};
            update();
        }

        void setCornerImageSizeRatio(double value)
        {
            m_cornerImageSizeRatio=value;
            update();
        }

        double cornerImageSizeRatio() const noexcept
        {
            return m_cornerImageSizeRatio;
        }

        void setRightBottomCircleColor(QColor color)
        {
            m_rightBottomCircleColor=color;
            update();
        }

        QColor rightBottomCircleColor() const noexcept
        {
            return m_rightBottomCircleColor;
        }

        void setCornerImageOffsets(int x, int y)
        {
            m_cornerImageXOffset=x;
            m_cornerImageYOffset=y;
            update();
        }

        void setRightBottomCircleRadius(int r)
        {
            m_rightBottomCircleRadius=r;
        }

        int rightBottomCircleDiameter() const
        {
            if (m_rightBottomCircleRadius!=0)
            {
                return m_rightBottomCircleRadius*2;
            }

            return qRound(width() * m_cornerImageSizeRatio);
        }

        void setAvatarName(std::string name)
        {
            m_avatarName=std::move(name);
        }

        std::string avatarName() const
        {
            return m_avatarName;
        }

        void setAvatarSize(QSize size)
        {
            setImageSize(size);
        }

        QSize avatarSize() const
        {
            return imageSize();
        }

        template <typename PathT>
        void setAvatarPath(PathT path)
        {
            setImagePath(std::move(path));
            updateBackgroundColor();
        }

        const auto& avatarPath() const noexcept
        {
            return path();
        }

        void setAvatarSource(std::shared_ptr<AvatarSource> avatarSource)
        {
            m_avatarSource=avatarSource.get();
            setImageSource(std::move(avatarSource));
            updateBackgroundColor();
        }

        QColor fontColor() const
        {
            //! @todo Use from avatar source
            return m_fontColor;
        }

        void setFontColor(QColor color)
        {
            m_fontColor=color;
        }

        double fontSizeRatio() const
        {
            //! @todo Use from avatar source
            return m_fontSizeRatio;
        }

        void setFontSizeratio(double ratio)
        {
            m_fontSizeRatio=ratio;
        }

        void setBottomPixmapHoverVisible(bool enable)
        {
            m_bottomPixmapHoverVisible=enable;
        }

        bool isBottomPixmapHoverVisible() const
        {
            return m_bottomPixmapHoverVisible;
        }

    protected:

        void doPaint(QPainter* painter) override;
        void fillIfNoPixmap(QPainter*) override;

        virtual void updateBackgroundColor();

        void enterEvent(QEnterEvent* event) override;
        void leaveEvent(QEvent* event) override;

    private:

        void generateLetters(QPainter* painter) const;

        bool m_rightBottomCircle=false;
        QPixmap m_rightBottomPixmap;
        std::shared_ptr<SvgIcon> m_rightBottomSvgIcon;
        double m_cornerImageSizeRatio=CornerImageSizeRatio;
        QColor m_rightBottomCircleColor=DefaultCornerCircleColor;

        int m_cornerImageXOffset=0;
        int m_cornerImageYOffset=0;

        int m_rightBottomCircleRadius=0;

        std::string m_avatarName;

        AvatarSource* m_avatarSource=nullptr;
        QColor m_backgroundColor=QRgb(0x00669bbc);
        QColor m_fontColor=DefaultFontColor;
        double m_fontSizeRatio=DefaultFontSizeRation;
        bool m_bottomPixmapHoverVisible=false;
        bool m_hovered;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_AVATAR_HPP
