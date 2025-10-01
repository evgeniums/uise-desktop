/**
@copyright Evgeny Sidorov 2022

This software is dual-licensed. Choose the appropriate license for your project.

1. The GNU GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-GPLv3.md](LICENSE-GPLv3.md) or copy at https://www.gnu.org/licenses/gpl-3.0.txt)

2. The GNU LESSER GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-LGPLv3.md](LICENSE-LGPLv3.md) or copy at https://www.gnu.org/licenses/lgpl-3.0.txt).

You may select, at your option, one of the above-listed licenses.

*/

/****************************************************************************/

/** @file uise/desktop/avatar.cpp
*
*  Defines avatar image source.
*
*/

/****************************************************************************/

#include <QCryptographicHash>
#include <QPainter>

#include <uise/desktop/avatar.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

static const char* ColorPallette[]={"#ffbe0b","#fb5607","#ff006e","#8338ec","#3a86ff"};

/************************** Avatar **********************************/

//--------------------------------------------------------------------------

Avatar::Avatar()
    : m_imageSource(nullptr),
      m_refCount(0),
      m_backgroundColor(Qt::blue)
{}

//--------------------------------------------------------------------------

Avatar::~Avatar()
{}

//--------------------------------------------------------------------------

void Avatar::updateProducers()
{
    if (!m_imageSource)
    {
        return;
    }

    auto id=QString::fromUtf8(m_avatarId);
    auto producers=m_imageSource->producers(id);
    for (auto& producer: producers)
    {
        producer->setPixmap(pixmap(producer->size()));
    }
}

//--------------------------------------------------------------------------

void Avatar::updateBackgroundColor()
{
    if (m_imageSource==nullptr || m_imageSource->backgroundPallette().empty())
    {
        return;
    }

    QCryptographicHash hash{QCryptographicHash::Sha1};
    hash.addData(m_avatarId);
    auto result=hash.result();

    size_t idx=0;
    memcpy(&idx,result.constData(),sizeof(idx));

    size_t palletteLength = m_imageSource->backgroundPallette().size();
    idx=idx%palletteLength;

    m_backgroundColor=m_imageSource->backgroundPallette().at(idx);
}

//--------------------------------------------------------------------------

QPixmap Avatar::generateLetterPixmap(const QSize& size) const
{
    QPixmap px(size);

    QColor color{Qt::white};
    QString fontName{AvatarSource::DefaultFontName};
    size_t fontSize=AvatarSource::DefaultFontSize;
    size_t maxLetters=AvatarSource::DefaultMaxAvatarLetterCount;
    int xRadius=px.width()/2;
    int yRadius=px.height()/2;

    if (m_imageSource!=nullptr)
    {
        color=m_imageSource->fontColor();
        fontName=m_imageSource->fontName();
        fontSize=m_imageSource->fontSize();
        maxLetters=m_imageSource->maxAvatarLetterCount();
        xRadius=m_imageSource->evalXRadius(px.width());
        yRadius=m_imageSource->evalXRadius(px.height());
    }

    QPainter painter(&px);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setBrush(m_backgroundColor);
    painter.drawRoundedRect(0, 0, px.width(), px.height(), xRadius, yRadius);
    painter.setPen(color);
    QFont font(fontName,fontSize, QFont::Bold);
    painter.setFont(font);

    QString name=QString::fromUtf8(m_avatarName);
    auto firstLetters=name.split(" ");
    auto count=std::min(firstLetters.size(),qsizetype(maxLetters));
    firstLetters.resize(count);
    auto letters=firstLetters.join("");

    painter.drawText(px.rect(), Qt::AlignCenter, letters);
    painter.end();

    return px;
}

//--------------------------------------------------------------------------

QPixmap Avatar::pixmap(const QSize& size) const
{
    if (m_basePixmap.isNull())
    {
        return generateLetterPixmap(size);
    }

    if (m_imageSource==nullptr)
    {
        return m_basePixmap.scaled(size,AvatarSource::DefaultAspectRatioMode,Qt::SmoothTransformation);
    }
    return m_basePixmap.scaled(size,m_imageSource->aspectRatioMode(),Qt::SmoothTransformation);
}

//--------------------------------------------------------------------------

void Avatar::updateGeneratedAvatar()
{
    if (m_basePixmap.isNull())
    {
        updateProducers();
    }
}

/************************** AvatarSource *****************************/

//--------------------------------------------------------------------------

AvatarSource::AvatarSource()
    : m_fontName(DefaultFontName),
      m_fontSize(DefaultFontSize),
      m_fontColor(Qt::white),
      m_maxAvatarLetterCount(DefaultMaxAvatarLetterCount)
{
    for (size_t i=0;i<sizeof(ColorPallette);i++)
    {
        auto color=QColor::fromString(ColorPallette[i]);
        if (color.isValid())
        {
            m_backgroundPallette.emplace_back(color);
        }
    }

    if (m_backgroundPallette.empty())
    {
        m_backgroundPallette.emplace_back(Qt::blue);
    }
}

//--------------------------------------------------------------------------

void AvatarSource::doLoadProducer(const PixmapKey& key)
{
    auto it=m_avatars.find(key.name);
    if (it==m_avatars.end())
    {
        auto avatar=m_avatarBuilder(key.name);
        avatar->setAvatarId(key.name.toStdString());
        avatar->setImageSource(this);
        avatar->incRefCount();
        m_avatars.emplace(key.name,std::move(avatar));
    }
    else
    {
        it->second->incRefCount();
    }
}

//--------------------------------------------------------------------------

void AvatarSource::doUnloadProducer(const PixmapKey& key)
{
    auto it=m_avatars.find(key.name);
    if (it!=m_avatars.end())
    {
        it->second->decRefCount();
        if (it->second->refCount()<=0)
        {
            m_avatars.erase(it);
        }
    }
}

//--------------------------------------------------------------------------

void AvatarSource::doLoadPixmap(const PixmapKey& key)
{
    auto it=m_avatars.find(key.name);
    if (it!=m_avatars.end())
    {
        auto px=it->second->pixmap(key.size);
        updatePixmap(key,px);
    }
}

//--------------------------------------------------------------------------

void AvatarSource::clearAvatars()
{
    m_avatars.clear();
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
