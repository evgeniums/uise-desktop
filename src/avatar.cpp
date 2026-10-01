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
#include <QMouseEvent>

#include <uise/desktop/utils/datetime.hpp>
#include <uise/desktop/style.hpp>
#include <uise/desktop/colorpalette.hpp>
#include <uise/desktop/avatar.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

//! Built-in fallback, used only when the loaded style defines no non-empty "avatar-background"
//! palette (see ColorPaletteNames and resources/style/{light,dark}/palettes.json, which carry the
//! same list). Keep the two in sync.
//!
//! Chosen to stay clearly distinguishable (CIEDE2000 >= 20) from the surfaces an avatar is drawn
//! on, in both themes: list rows and their hover/selected states, the chat view and, notably,
//! the selected tab (#218DAE light, #1e6091 dark). That rules out every blue and teal, so there
//! are none -- re-check against the tab/list colours before adding one.
static const char* ColorPallette[]={"#fb5607","#ff006e","#8338ec","#9C4A1A",
                                    "#390099","#ce4257","#519A66","#386641",
                                    "#A16207","#B5179E","#C1121F","#15803D",
                                    "#D53F8C","#B56576","#7C6A0A"
                                    };

namespace {

std::vector<QColor> fallbackBackgroundPallette()
{
    std::vector<QColor> result;
    auto colorCount=sizeof(ColorPallette)/sizeof(ColorPallette[0]);
    for (size_t i=0;i<colorCount;i++)
    {
        auto color=QColor::fromString(ColorPallette[i]);
        if (color.isValid())
        {
            result.emplace_back(color);
        }
    }
    return result;
}

//! The palette from the loaded style, so a white label or an opaque restyle can override it per
//! colour theme; the built-in list when the style defines none.
std::vector<QColor> styleBackgroundPallette()
{
    auto pallette=Style::instance().colorPalette(ColorPaletteNames::AvatarBackground);
    if (pallette.empty())
    {
        pallette=fallbackBackgroundPallette();
    }
    return pallette;
}

}

/********************* AvatarBackgroundGenerator ********************/

//--------------------------------------------------------------------------

AvatarBackgroundGenerator::AvatarBackgroundGenerator(std::vector<QColor> backgroundPallette)
{
    if (!backgroundPallette.empty())
    {
        m_backgroundPallette=std::move(backgroundPallette);
        return;
    }

    m_backgroundPallette=styleBackgroundPallette();
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
    auto colorIdx=paletteIndex(hash.result(),m_backgroundPallette.size());
    return m_backgroundPallette.at(colorIdx);
}

/************************** Avatar **********************************/

//--------------------------------------------------------------------------

Avatar::Avatar()
    : m_avatarSource(nullptr),
      m_backgroundColor(AvatarBackgroundGenerator::DefaultBackgroundColor)
{}

//--------------------------------------------------------------------------

Avatar::~Avatar()
{}

//--------------------------------------------------------------------------

void Avatar::refreshGeneratedBackground()
{
    updateBackgroundColor();
    // generated pixmaps are never cached (see pixmap()), so re-pushing is all it takes
    updateGeneratedAvatar();
}

//--------------------------------------------------------------------------

void Avatar::updateProducers(bool forBasePixmap, bool nameData)
{
    if (!m_avatarSource)
    {
        return;
    }

#if 0
    qDebug() << "Avatar::updateProducers() " << toString() << printCurrentDateTime();
#endif

    auto producers=m_avatarSource->producers(avatarPath());
    for (auto& producer: producers)
    {
        if (nameData)
        {
            producer->setData(QString::fromStdString(m_avatarName));
        }
        else
        {
            if (forBasePixmap)
            {
                auto it=m_pixmaps.find(m_basePixmap.size());
                if (it!=m_pixmaps.end())
                {
                    continue;
                }
            }

            producer->setPixmap(pixmap(producer->size()));
        }
    }
}

//--------------------------------------------------------------------------

void Avatar::setPixmap(const QPixmap& pixmap)
{
    // qDebug() << "Avatar::setPixmap() " << toString() << printCurrentDateTime();

    m_pixmaps.emplace(pixmap.size(),pixmap);
    updateProducers();
}

//--------------------------------------------------------------------------

QPixmap Avatar::pixmap(const QSize& size) const
{
    if (!size.isValid())
    {
        return QPixmap{};
    }

    const qreal pixelRatio = qApp->primaryScreen()->devicePixelRatio();

    auto it=m_pixmaps.find(size);
    if (it!=m_pixmaps.end())
    {
#if 0
        qDebug() << "Avatar::pixmap using cached pixmap " << printCurrentDateTime() << " " << toString() << " size " << size;
#endif
        auto px=it->second;
        px.setDevicePixelRatio(pixelRatio);
        return px;
    }

    // generate pixmap if base pixmap is not set
    if (m_basePixmap.isNull())
    {
#if 0
        qDebug() << "Avatar::pixmap using generated pixmap " << printCurrentDateTime() << " " <<toString() << " size " << size;
#endif
        return generatePixmap(size);
    }

    // scale base pixmap if needed
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

    // use scaled pixmap
#if 0
    qDebug() << "Avatar::pixmap using scaled pixmap " << printCurrentDateTime() << " " << toString() << " size " << size;
#endif
    auto px=scalePixmap();
    if (px.isNull())
    {
        return px;
    }

    // set device pixel ratio because input size must be with pixel ratio
    px.setDevicePixelRatio(pixelRatio);

    // done
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

void Avatar::setAvatarName(std::string name)
{
    m_avatarName=std::move(name);
    updateProducers(false,true);
    updateGeneratedAvatar();
}

//--------------------------------------------------------------------------

void Avatar::updateGeneratedAvatar()
{
    if (m_basePixmap.isNull())
    {
        // qDebug() << "Avatar::updateGeneratedAvatar() " << toString() << printCurrentDateTime();

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

namespace {
QString getFirstInitial(const QString &name)
{
    if (name.isEmpty()) return QString();

    QVector<uint> ucs4 = name.toUcs4();
    uint firstCodePoint = ucs4[0];

    // Use reinterpret_cast to satisfy the char32_t* overload
    return QString::fromUcs4(reinterpret_cast<const char32_t*>(&firstCodePoint), 1).toHtmlEscaped();
}
}

QPixmap Avatar::generatePixmap(const QSize& size) const
{
    if (!size.isValid())
    {
        return QPixmap{};
    }

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
        QFont font;
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
        for (qsizetype i=0;i<words.count();i++)
        {
            auto& word=words.at(i);
            letters+=getFirstInitial(word).toUpper();
        }
        // 1. Get metrics for the font you are using
        QFontMetrics fm(painter.font());

        // 2. Calculate the VISUAL height of the letters (ignoring empty space)
        int visualHeight = fm.capHeight();

        // 3. Prepare the text
        QStaticText text("<div align='center'>" + letters + "</div>");
        text.setTextWidth(size.width()); // Handles horizontal centering

        // 4. THE FIX: The Vertical Offset
        // We want the MIDDLE of the capHeight to be at the MIDDLE of the circle.
        // Since drawStaticText draws from the TOP, we subtract the internal 'ascent'.
        int y = (size.height() / 2) - (visualHeight / 2);

        // 5. If it is still too low, it's because QStaticText adds a
        // default 1-2px margin for HTML. Force it up:
        y -= (fm.ascent() - visualHeight);

        // empiric offset
        y+=1;

        painter.drawStaticText(0, y, text);
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
    m_backgroundColorGenerator=std::make_shared<AvatarBackgroundGenerator>();
    reloadBackgroundPallette();

    // Style is not a QObject: this is how a theme switch reaches us (see Style::addPalettesChangedHandler()).
    m_palettesHandlerId=Style::instance().addPalettesChangedHandler(
        [this]()
        {
            reloadBackgroundPallette();
        }
    );
}

//--------------------------------------------------------------------------

AvatarSource::~AvatarSource()
{
    Style::instance().removePalettesChangedHandler(m_palettesHandlerId);
}

//--------------------------------------------------------------------------

void AvatarSource::reloadBackgroundPallette()
{
    // Whatever the application set explicitly is left alone.
    if (!m_ownPallette && !m_ownGenerator)
    {
        return;
    }

    auto pallette=styleBackgroundPallette();
    if (pallette.empty())
    {
        pallette.emplace_back(Qt::blue);
    }

    auto changed=false;
    if (m_ownPallette && m_backgroundPallette!=pallette)
    {
        m_backgroundPallette=pallette;
        changed=true;
    }
    if (m_ownGenerator && m_backgroundColorGenerator && m_backgroundColorGenerator->backgroundPallette()!=pallette)
    {
        m_backgroundColorGenerator->setBackgroundPallette(pallette);
        changed=true;
    }

    // The default palettes are the same in every shipped theme, so a theme switch usually changes nothing.
    if (changed)
    {
        for (auto& el: m_avatars)
        {
            el.second->refreshGeneratedBackground();
        }
    }
}

//--------------------------------------------------------------------------

void AvatarSource::doLoadProducer(const PixmapKey& key)
{
    const auto& avatarPath=key.toWithPath();

    auto it=m_avatars.find(avatarPath);
    if (it==m_avatars.end())
    {
        Q_ASSERT(m_avatarBuilder);
        auto avatar=m_avatarBuilder(avatarPath);
        avatar->setAvatarPath(avatarPath);
        avatar->setAvatarSource(this);
        avatar->watchPixmapForSize(key.size());
        m_avatars.emplace(avatarPath,std::move(avatar));
    }
    else
    {
        it->second->watchPixmapForSize(key.size());
    }
}

//--------------------------------------------------------------------------

void AvatarSource::doUnloadProducer(const PixmapKey& key)
{
    auto it=m_avatars.find(key.toWithPath());
    if (it!=m_avatars.end())
    {
        it->second->unwatchPixmapForSize(key.size());
        if (it->second->watchPixmapSizes().empty())
        {
            m_avatars.erase(it);
        }
    }
}

//--------------------------------------------------------------------------

void AvatarSource::doLoadPixmap(const PixmapKey& key)
{
    auto it=m_avatars.find(key.toWithPath());
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

    int x=0;
    int y=0;

    switch (m_cornerImagePosition)
    {
        case Qt::TopLeftCorner:
            x=m_cornerImageXOffset;
            y=m_cornerImageYOffset;
            break;
        case Qt::TopRightCorner:
            x=width() - w - m_cornerImageXOffset;
            y=m_cornerImageYOffset;
            break;
        case Qt::BottomLeftCorner:
            x=m_cornerImageXOffset;
            y=height() - h - m_cornerImageYOffset;
            break;
        case Qt::BottomRightCorner:
            x=width() - w - m_cornerImageXOffset;
            y=height() - h - m_cornerImageYOffset;
            break;
    }

    QRect rect{x,y,w,h};
    auto sz=QSize{w,h};

    if (!m_bottomPixmapHoverVisible || m_hovered)
    {
        auto pixmap=m_cornerPixmap;
        if (pixmap.isNull() && m_cornerSvgIcon)
        {
            const qreal pixelRatio = qApp->primaryScreen()->devicePixelRatio();
            sz=sz*pixelRatio;
            pixmap=m_cornerSvgIcon->pixmap(sz,currentSvgIconMode());
        }
        if (pixmap.isNull())
        {
            return;
        }
        painter->setPen(Qt::NoPen);
        painter->setRenderHints(QPainter::SmoothPixmapTransform);
        painter->drawPixmap(rect,pixmap);
    }
}

//--------------------------------------------------------------------------

void AvatarWidget::updateBackgroundColor()
{
    if (m_avatarSource==nullptr || m_avatarSource->backgroundPallette().empty())
    {
        return;
    }

    QCryptographicHash hash{QCryptographicHash::Sha1};
    for (const auto& el: avatarPath().path())
    {
        hash.addData(el);
    }
    auto colorIdx=paletteIndex(hash.result(),m_avatarSource->backgroundPallette().size());
    m_backgroundColor=m_avatarSource->backgroundPallette().at(colorIdx);
}

//--------------------------------------------------------------------------

void AvatarWidget::changeEvent(QEvent* event)
{
    RoundedImage::changeEvent(event);

    // The avatar source reloads its palette when the style does, which is before the new style
    // sheet is applied, so by the time StyleChange arrives the palette is already the new theme's.
    if (event->type()==QEvent::StyleChange)
    {
        auto prev=m_backgroundColor;
        updateBackgroundColor();
        if (prev!=m_backgroundColor)
        {
            update();
        }
    }
}

//--------------------------------------------------------------------------

QPixmap AvatarWidget::nonamePixmap(const QSize& size, QRect* rect) const
{
    if (!m_avatarSource || !m_avatarSource->noNameSvgIcon())
    {
        return QPixmap{};
    }

    const qreal pixelRatio = qApp->primaryScreen()->devicePixelRatio();
    auto sz=size * NonameImageSizeRatio;
    QRect r{size.width()/2-sz.width()/2,size.height()/2-sz.height()/2,sz.width(),sz.height()};
    if (rect!=nullptr)
    {
        *rect=r;
    }
    auto px=m_avatarSource->noNameSvgIcon()->pixmap(sz*pixelRatio,currentSvgIconMode());
    if (!px.isNull())
    {
        px.setDevicePixelRatio(pixelRatio);
    }
    return px;
}

//--------------------------------------------------------------------------

void AvatarWidget::fillIfNoPixmap(QPainter* painter)
{
    const qreal pixelRatio = qApp->primaryScreen()->devicePixelRatio();
    painter->setRenderHint(QPainter::Antialiasing,true);
    painter->setBrush(m_backgroundColor);
    painter->setPen(Qt::NoPen);
    painter->drawRoundedRect(0, 0, width(), height(), xRadius(), yRadius());

    if (!m_avatarName.empty())
    {        
        QPixmap px{imageSize()};
        px.fill(Qt::transparent);
        QPainter pxp;
        pxp.begin(&px);
        pxp.setRenderHints(QPainter::TextAntialiasing);
        generateLetters(&pxp);
        pxp.end();
        px.setDevicePixelRatio(pixelRatio);
        painter->drawPixmap(rect(),px);
    }
    QRect r;
    auto px=nonamePixmap(size(),&r);
    if (!px.isNull())
    {
        painter->drawPixmap(r,px);
    }
}

//--------------------------------------------------------------------------

void AvatarWidget::generateLetters(QPainter* painter) const
{
    QString fontName{AvatarSource::DefaultFontName};
    size_t maxLetters=AvatarSource::DefaultMaxAvatarLetterCount;

    painter->setPen(fontColor());
    auto fontSize=imageSize().height()*fontSizeRatio();
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
    for (qsizetype i=0;i<words.count();i++)
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
    auto x= imageSize().width()/2 - qCeil(fw/2);
    auto y= imageSize().height()/2 - qCeil(fh/2);
    auto bearing=metrics.leftBearing(letters[0]);

    painter->drawStaticText(x-bearing,y-dy,QStaticText{letters});
}

//--------------------------------------------------------------------------

void AvatarWidget::enterEvent(QEnterEvent* event)
{
    RoundedImage::enterEvent(event);
    m_hovered=true;
    update();
}

//--------------------------------------------------------------------------

void AvatarWidget::leaveEvent(QEvent* event)
{
    RoundedImage::leaveEvent(event);
    m_hovered=false;
    update();
}

//--------------------------------------------------------------------------

void AvatarWidget::mousePressEvent(QMouseEvent* event)
{
    if (m_clickable && event->button()==Qt::LeftButton)
    {
        emit clicked();
    }
    RoundedImage::mousePressEvent(event);
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
