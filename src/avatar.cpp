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
#include <QGuiApplication>
#include <QScreen>

#include <uise/desktop/avatar.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

static const char* ColorPallette[]={"#ffbe0b","#fb5607","#ff006e","#8338ec","#3a86ff",
                                    "#00a6fb", "#006494", "ef476f", "#006d77", "#e36414",
                                    "#2a9d8f", "#fa9500", "#390099", "#ce4257", "#52796f"
                                    };

/********************* AvatarBackgroundGenerator ********************/

//--------------------------------------------------------------------------

AvatarBackgroundGenerator::AvatarBackgroundGenerator(std::vector<QColor> backgroundPallette)
{
    if (!backgroundPallette.empty())
    {
        m_backgroundPallette=std::move(backgroundPallette);
        return;
    }

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
        m_backgroundPallette.emplace_back(DefaultBackgroundColor);
    }
}

//--------------------------------------------------------------------------

AvatarBackgroundGenerator::~AvatarBackgroundGenerator()
{}

//--------------------------------------------------------------------------

QColor AvatarBackgroundGenerator::generateBackgroundColor(const WithPath& path) const
{
    if (m_backgroundPallette.empty())
    {
        return DefaultBackgroundColor;
    }

    QCryptographicHash hash{QCryptographicHash::Sha1};
    for (const auto& el: path.path())
    {
        hash.addData(el);
    }
    auto result=hash.result();

    size_t idx=0;
    memcpy(&idx,result.constData(),sizeof(idx));

    size_t palletteLength = m_backgroundPallette.size();
    auto colorIdx=idx%palletteLength;

    return m_backgroundPallette.at(colorIdx);
}

/************************** Avatar **********************************/

//--------------------------------------------------------------------------

Avatar::Avatar()
    : m_avatarSource(nullptr),
      m_refCount(0),
      m_backgroundColor(AvatarBackgroundGenerator::DefaultBackgroundColor)
{}

//--------------------------------------------------------------------------

Avatar::~Avatar()
{}

//--------------------------------------------------------------------------

void Avatar::updateProducers()
{
    if (!m_avatarSource)
    {
        return;
    }

    auto producers=m_avatarSource->producers(avatarPath());
    for (auto& producer: producers)
    {
        producer->setPixmap(pixmap(producer->size()));
    }
}

//--------------------------------------------------------------------------

QPixmap Avatar::pixmap(const QSize& size) const
{
    // generate pixmap if base pixmap is not set
    if (m_basePixmap.isNull())
    {
        return generatePixmap(size);
    }

    // scale pixmap if needed
    auto scalePixmap=[&,this]()
    {
        if (m_basePixmap.size()==size)
        {
            return m_basePixmap;
        }

        auto aspectRatio=AvatarSource::DefaultAspectRatioMode;
        if (m_avatarSource!=nullptr)
        {
            aspectRatio=m_avatarSource->aspectRatioMode();
        }
        return m_basePixmap.scaled(size,aspectRatio,Qt::SmoothTransformation);
    };

    // use pixmap
    auto px=scalePixmap();
    if (px.isNull())
    {
        return px;
    }

    // set device pixel ratio because input size must be with pixel ratio
    const qreal pixelRatio = qApp->primaryScreen()->devicePixelRatio();
    px.setDevicePixelRatio(pixelRatio);
    return px;
}

//--------------------------------------------------------------------------

void Avatar::updateBackgroundColor()
{
    auto generator=backgroundColorGenerator();
    if (generator)
    {
        m_backgroundColor=generator->generateBackgroundColor(*this);
    }
    else
    {
        m_backgroundColor=AvatarBackgroundGenerator::DefaultBackgroundColor;
    }
}

//--------------------------------------------------------------------------

void Avatar::updateGeneratedAvatar()
{
    if (m_basePixmap.isNull())
    {
        updateProducers();
    }
}

//--------------------------------------------------------------------------

QColor Avatar::fontColor() const
{
    if (m_fontColor || !m_avatarSource)
    {
        return m_fontColor.value_or(DefaultFontColor);
    }
    return m_avatarSource->fontColor();
}

//--------------------------------------------------------------------------

double Avatar::fontSizeRatio() const
{
    if (m_fontSizeRatio || !m_avatarSource)
    {
        return m_fontSizeRatio.value_or(DefaultFontSizeRatio);
    }
    return m_avatarSource->fontSizeRatio();
}

//--------------------------------------------------------------------------

std::shared_ptr<AvatarBackgroundGenerator> Avatar::backgroundColorGenerator() const
{
    if (m_avatarSource==nullptr)
    {
        return std::shared_ptr<AvatarBackgroundGenerator>{};
    }

    if (m_backgroundColorGenerator)
    {
        return m_backgroundColorGenerator;
    }
    return m_avatarSource->backgroundColorGenerator();
}

//--------------------------------------------------------------------------

QPixmap Avatar::generatePixmap(const QSize& size) const
{
    QString fontName{AvatarSource::DefaultFontName};
    size_t maxLetters=AvatarSource::DefaultMaxAvatarLetterCount;

    QPixmap px{size};
    px.fill(Qt::transparent);

    QPainter painter;
    painter.begin(&px);

    // draw background
    painter.setRenderHints(QPainter::Antialiasing);
    painter.setBrush(m_backgroundColor);
    painter.setPen(Qt::NoPen);
    painter.drawRect(0, 0, size.width(), size.height());

    if (!m_avatarName.empty())
    {
        // draw first letters
        painter.setRenderHints(QPainter::TextAntialiasing);
        painter.setPen(fontColor());
        auto fontSize=size.height()*fontSizeRatio();
        QFont font{fontName};
        font.setPixelSize(qRound(fontSize));
        font.setStyleStrategy(QFont::PreferAntialias);
        font.setBold(true);
        painter.setFont(font);

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
        auto x= size.width()/2 - qCeil(fw/2);
        auto y= size.height()/2 - qCeil(fh/2);
        auto bearing=metrics.leftBearing(letters[0]);

        painter.drawStaticText(x-bearing,y-dy,QStaticText{letters});
    }
    else if (m_avatarSource && m_avatarSource->noNameSvgIcon())
    {
        // draw noname icon
        painter.setRenderHints(QPainter::SmoothPixmapTransform);
        auto sz=size * 0.7;
        QRect r{size.width()/2-sz.width()/2,size.height()/2-sz.height()/2,sz.width(),sz.height()};
        m_avatarSource->noNameSvgIcon()->paint(&painter,r);
    }

    // done
    painter.end();
    const qreal pixelRatio = qApp->primaryScreen()->devicePixelRatio();
    px.setDevicePixelRatio(pixelRatio);
    return px;
}

/************************** AvatarSource *****************************/

//--------------------------------------------------------------------------

AvatarSource::AvatarSource()
    : m_fontName(DefaultFontName),
      m_fontColor(Avatar::DefaultFontColor),
      m_fontSizeRatio(Avatar::DefaultFontSizeRatio),
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

    m_backgroundColorGenerator=std::make_shared<AvatarBackgroundGenerator>();
}

//--------------------------------------------------------------------------

void AvatarSource::doLoadProducer(const PixmapKey& key)
{
    auto it=m_avatars.find(key);
    if (it==m_avatars.end())
    {
        Q_ASSERT(m_avatarBuilder);
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
    auto sz=QSize{w,h};

    auto pixmap=m_rightBottomPixmap;
    if (pixmap.isNull() && m_rightBottomSvgIcon)
    {
        pixmap=m_rightBottomSvgIcon->pixmap(imageSize(),currentSvgIconMode());
    }
    if (pixmap.isNull())
    {
        return;
    }
    painter->setPen(Qt::NoPen);
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

void AvatarWidget::fillIfNoPixmap(QPainter* painter)
{
    painter->setBrush(m_backgroundColor);
    painter->setPen(Qt::NoPen);
    painter->drawRoundedRect(0, 0, imageSize().width(), imageSize().height(), xRadius(), yRadius());

    if (!m_avatarName.empty())
    {
        const qreal pixelRatio = qApp->primaryScreen()->devicePixelRatio();
        QPixmap px{imageSize()*pixelRatio};
        px.fill(Qt::transparent);
        QPainter pxp;
        pxp.begin(&px);
        pxp.setRenderHints(QPainter::TextAntialiasing);
        generateLetters(&pxp);
        pxp.end();
        px.setDevicePixelRatio(pixelRatio);
        painter->drawPixmap(rect(),px);
    }
    else if (m_avatarSource && m_avatarSource->noNameSvgIcon())
    {
        auto sz=imageSize() * 0.7;
        QRect r{width()/2-sz.width()/2,height()/2-sz.height()/2,sz.width(),sz.height()};
        m_avatarSource->noNameSvgIcon()->paint(painter,r,currentSvgIconMode(),QIcon::Off,isCacheSvgPixmap());
    }
}

//--------------------------------------------------------------------------

void AvatarWidget::generateLetters(QPainter* painter) const
{
    QString fontName{AvatarSource::DefaultFontName};
    size_t maxLetters=AvatarSource::DefaultMaxAvatarLetterCount;

    painter->setPen(fontColor());
    const qreal pixelRatio = qApp->primaryScreen()->devicePixelRatio();
    auto fontSize=imageSize().height()*fontSizeRatio()*pixelRatio;
    QFont font{fontName};
    font.setPixelSize(qRound(fontSize));
    font.setStyleStrategy(QFont::PreferAntialias);
    font.setBold(true);
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
    auto x= width()*pixelRatio/2 - qCeil(fw/2);
    auto y= height()*pixelRatio/2 - qCeil(fh/2);
    auto bearing=metrics.leftBearing(letters[0]);

    painter->drawStaticText(x-bearing,y-dy,QStaticText{letters});
}

UISE_DESKTOP_NAMESPACE_END
