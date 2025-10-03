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
#include <QStaticText>

#include <uise/desktop/avatar.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

static const char* ColorPallette[]={"#ffbe0b","#fb5607","#ff006e","#8338ec","#3a86ff",
                                    "#00a6fb", "#006494", "ef476f", "#006d77", "#e36414",
                                    "#2a9d8f", "#fa9500", "#390099", "#ce4257", "#52796f"
                                    };

/************************** Avatar **********************************/

//--------------------------------------------------------------------------

Avatar::Avatar()
    : m_imageSource(nullptr),
      m_refCount(0)
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

    auto producers=m_imageSource->producers(avatarPath());
    for (auto& producer: producers)
    {
        producer->setPixmap(pixmap(producer->size()));
    }
}

//--------------------------------------------------------------------------

QPixmap Avatar::pixmap(const QSize& size) const
{
    if (m_basePixmap.isNull())
    {
        return QPixmap{};
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
      m_fontColor(Qt::white),
      m_maxAvatarLetterCount(DefaultMaxAvatarLetterCount)
{
    auto colorCount=sizeof(ColorPallette)/sizeof(ColorPallette[0]);
    for (size_t i=0;i<colorCount;i++)
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
    auto it=m_avatars.find(key);
    if (it==m_avatars.end())
    {
        auto avatar=m_avatarBuilder(key);
        avatar->setAvatarPath(key);
        avatar->setImageSource(this);
        avatar->incRefCount();
        m_avatars.emplace(key,std::move(avatar));
    }
    else
    {
        it->second->incRefCount();
    }
}

//--------------------------------------------------------------------------

void AvatarSource::doUnloadProducer(const PixmapKey& key)
{
    auto it=m_avatars.find(key);
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
    auto it=m_avatars.find(key);
    if (it!=m_avatars.end())
    {
        auto px=it->second->pixmap(key.size());
        if (!px.isNull())
        {
            updatePixmap(key,px);
            return;
        }
    }
}

//--------------------------------------------------------------------------

void AvatarSource::clearAvatars()
{
    m_avatars.clear();
}

/************************** AvatarWidget *****************************/

//--------------------------------------------------------------------------

void AvatarWidget::doPaint(QPainter* painter)
{
    if (m_rightBottomCircle)
    {
        painter->setPen(Qt::NoPen);
        auto d=rightBottomCircleDiameter();
        painter->setBrush(m_rightBottomCircleColor);
        painter->drawEllipse(width()-d-m_cornerImageXOffset, height()-d-m_cornerImageYOffset, d, d);
        return;
    }

    int w=qRound(width() * m_cornerImageSizeRatio);
    int h=qRound(height() * m_cornerImageSizeRatio);
    int x=width() - w - m_cornerImageXOffset;
    int y=height() - h - m_cornerImageYOffset;
    QRect rect{x,y,w,h};

    auto pixmap=m_rightBottomPixmap;
    if (pixmap.isNull() && m_rightBottomSvgIcon)
    {
        m_rightBottomSvgIcon->paint(painter,rect);
    }
    if (pixmap.isNull())
    {
        return;
    }

    if (pixmap.width()!=w || pixmap.height()!=h)
    {
        pixmap=pixmap.scaled(QSize(w,h),Qt::IgnoreAspectRatio,Qt::SmoothTransformation);
    }

    painter->setPen(Qt::NoPen);
    painter->setBrush(pixmap);    
    painter->drawPixmap(rect,pixmap);
}

//--------------------------------------------------------------------------

void AvatarWidget::updateBackgroundColor()
{
    if (m_avatarSource==nullptr || m_avatarSource->backgroundPallette().empty())
    {
        return;
    }

    QCryptographicHash hash{QCryptographicHash::Sha1};
    for (const auto& el: avatarPath())
    {
        hash.addData(el);
    }
    auto result=hash.result();

    size_t idx=0;
    memcpy(&idx,result.constData(),sizeof(idx));

    size_t palletteLength = m_avatarSource->backgroundPallette().size();
    auto colorIdx=idx%palletteLength;

    m_backgroundColor=m_avatarSource->backgroundPallette().at(colorIdx);
}

//--------------------------------------------------------------------------

void AvatarWidget::doFill(QPainter* painter, const QPixmap& pixmap)
{
    if (!pixmap.isNull())
    {
        RoundedImage::doFill(painter,pixmap);
        return;
    }

    painter->setBrush(m_backgroundColor);
    painter->setPen(Qt::NoPen);
    painter->drawRoundedRect(0, 0, imageSize().width(), imageSize().height(), xRadius(), yRadius());

    if (!m_avatarName.empty())
    {
        generateLetters(painter);
    }
    else if (m_avatarSource && m_avatarSource->noNameSvgIcon())
    {
        auto sz=imageSize() * 0.7;
        QRect r{width()/2-sz.width()/2,height()/2-sz.height()/2,sz.width(),sz.height()};
        m_avatarSource->noNameSvgIcon()->paint(painter,r);
    }
}

//--------------------------------------------------------------------------

void AvatarWidget::generateLetters(QPainter* painter) const
{
    QColor color{Qt::white};
    QString fontName{AvatarSource::DefaultFontName};
    size_t maxLetters=AvatarSource::DefaultMaxAvatarLetterCount;

    painter->setPen(color);
    auto fontSize=imageSize().height()*0.45;
    QFont font{fontName};
    font.setPointSize(qRound(fontSize));
    font.setStyleStrategy(QFont::PreferAntialias);    
    painter->setFont(font);

    QString name=QString::fromUtf8(m_avatarName);
    name=name.trimmed();
    auto words=name.split(" ");
    auto count=std::min(words.size(),qsizetype(maxLetters));
    words.resize(count);
    QString letters;
    for (size_t i=0;i<words.count();i++)
    {
        const auto& word=words.at(i);
        letters+=word.front().toUpper();
    }

    QFontMetrics metrics(font);
    auto br=metrics.tightBoundingRect(letters);
    auto bbr=metrics.boundingRect(letters);
    auto fw=std::min(br.width(),bbr.width());
    auto fh=br.height();
    auto dy=metrics.ascent()-br.height();
    auto x= width()/2 - qCeil(fw/2);
    auto y= height()/2 - qCeil(fh/2);
    auto bearing=metrics.leftBearing(letters[0]);

    painter->drawStaticText(x-bearing,y-dy-2,QStaticText{letters});
}

UISE_DESKTOP_NAMESPACE_END
