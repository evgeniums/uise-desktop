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
#include <uise/desktop/roundedimage.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

class AvatarSource;

class UISE_DESKTOP_EXPORT Avatar : public WithPath
{
    public:

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

        template <typename PathT>
        void setAvatarPath(PathT path)
        {
            setPath(std::move(path));
            updateBackgroundColor();
            updateGeneratedAvatar();
        }

        std::string avatarName() const
        {
            return m_avatarName;
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

        QPixmap basepPixmap() const
        {
            return m_basePixmap;
        }

        QPixmap generateLetterPixmap(const QSize& size) const;

        QPixmap pixmap(const QSize& size) const;

        void setImageSource(AvatarSource* imageSource) noexcept
        {
            m_imageSource=imageSource;
            updateBackgroundColor();
            updateGeneratedAvatar();
        }

        AvatarSource* imageSource() const noexcept
        {
            return m_imageSource;
        }

        void incRefCount()
        {
            ++m_refCount;
        }

        void decRefCount()
        {
            --m_refCount;
        }

        int refCount() const noexcept
        {
            return m_refCount;
        }

        QColor backgroundColor() const noexcept
        {
            return m_backgroundColor;
        }

    protected:

        virtual void updateBackgroundColor();

        void updateProducers();

    private:

        void updateGeneratedAvatar();

        std::string m_avatarName;

        QColor m_backgroundColor;
        QPixmap m_basePixmap;

        AvatarSource* m_imageSource;

        int m_refCount;
};

class UISE_DESKTOP_EXPORT AvatarSource : public RoundedImageSource
{
    public:

        constexpr static const char* DefaultFontName="Verdana";
        constexpr static const size_t DefaultMaxAvatarLetterCount=2;

        using AvatarBuilderFn=std::function<std::shared_ptr<Avatar> (const WithPath&)>;

        AvatarSource();

        //! @todo Configure font and background color from Style

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

    protected:

        void doLoadProducer(const PixmapKey& key) override;

        void doUnloadProducer(const PixmapKey& key) override;

        void doLoadPixmap(const PixmapKey& key) override;

    private:

        std::map<WithPath,std::shared_ptr<Avatar>> m_avatars;

        QString m_fontName;
        QColor m_fontColor;
        size_t m_maxAvatarLetterCount;
        std::vector<QColor> m_backgroundPallette;

        AvatarBuilderFn m_avatarBuilder;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_AVATAR_HPP
