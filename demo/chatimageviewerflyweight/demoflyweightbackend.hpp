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

/** @file demo/chatimageviewerflyweight/demoflyweightbackend.hpp
*
*  Synthetic dataset, fake version-ladder PixmapSource, and fake paged chat-history fetcher backing
*  demo/chatimageviewerflyweight/main.cpp -- split into its own header (rather than defined inline
*  in main.cpp with an "#include main.moc") so DemoChatFetcher's Q_OBJECT is picked up by the normal
*  AUTOMOC header-scanning path, same as every other Q_OBJECT class in this codebase.
*
*/

/****************************************************************************/

#ifndef UISE_DEMO_CHATIMAGEVIEWERFLYWEIGHT_BACKEND_HPP
#define UISE_DEMO_CHATIMAGEVIEWERFLYWEIGHT_BACKEND_HPP

#include <vector>
#include <deque>
#include <memory>

#include <QObject>
#include <QPainter>
#include <QImage>
#include <QPixmap>
#include <QDateTime>
#include <QTimer>
#include <QRandomGenerator>
#include <QPointer>
#include <QString>

#include <uise/desktop/chatimageviewer.hpp>

using namespace UISE_DESKTOP_NAMESPACE;

//--------------------------------------------------------------------------

// Same synthetic-gradient recipe as demo/chatimageviewer/main.cpp's own makeSampleImage() --
// no asset files needed.
inline QPixmap makeSampleImage(const QSize& size, const QColor& c1, const QColor& c2, const QString& label)
{
    QImage img(size,QImage::Format_ARGB32);
    img.fill(Qt::transparent);
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing);
    QLinearGradient grad(0,0,size.width(),size.height());
    grad.setColorAt(0,c1);
    grad.setColorAt(1,c2);
    p.fillRect(img.rect(),grad);
    p.setPen(Qt::white);
    auto font=p.font();
    font.setPointSize(18);
    font.setBold(true);
    p.setFont(font);
    p.drawText(img.rect(),Qt::AlignCenter,label);
    p.end();
    return QPixmap::fromImage(img);
}

//--------------------------------------------------------------------------

// A blurry low-rung stand-in: render small, then scale back up without smoothing -- visibly
// blocky/blurry next to the sharp rung, same idea as an actual thumbnail-then-full upgrade.
inline QPixmap makeBlurryImage(const QSize& size, const QColor& c1, const QColor& c2)
{
    auto small=makeSampleImage(QSize(24,18),c1,c2,QString());
    return small.scaled(size,Qt::IgnoreAspectRatio,Qt::FastTransformation);
}

//--------------------------------------------------------------------------

//! Resource path of the shared demo GIF asset, embedded into this demo's own binary via
//! chatimageviewerflyweightdemo.qrc (which references demo/imagelabel/assets/animated.gif by
//! relative path rather than duplicating the file).
inline QString animatedGifResourcePath()
{
    return QStringLiteral(":/uise/desktop/demo/chatimageviewerflyweight/animated.gif");
}

//! One synthetic image's full record -- the fake backend's whole "database".
struct ImageRecord
{
    QString messageId;
    QString sender;
    QDateTime dateTime;
    QColor c1;
    QColor c2;
    QString label;

    //! Whether DemoFlyweightSource::doLoadPixmap() also delivers animation content for this
    //! image, alongside its ordinary poster pixmap -- see buildDataset() for which images this
    //! is set on.
    bool isAnimated=false;
};

//! Path prefix images are addressed under -- see keyForIndex()/indexFromKey().
inline const QString& demoKeyPrefix()
{
    static const QString prefix=QStringLiteral("flyweight-image-");
    return prefix;
}

inline PixmapKey keyForIndex(int index)
{
    // A real size, not the default-constructed invalid one -- DemoFlyweightSource::doLoadPixmap()
    // renders at key.size(), and an invalid QSize would make every QPixmap::scaled()/setPixmap()
    // call produce a null pixmap (the exact anySize trap fixed elsewhere in this refactor, B2/B3).
    PixmapKey key{std::string("flyweight-image-")+std::to_string(index)};
    key.setSize(QSize(640,480));
    return key;
}

//! -1 if key does not look like one of ours.
inline int indexFromKey(const PixmapKey& key)
{
    if (key.path().size()!=1)
    {
        return -1;
    }
    auto path=QString::fromStdString(key.path().front());
    if (!path.startsWith(demoKeyPrefix()))
    {
        return -1;
    }
    bool ok=false;
    auto idx=path.mid(demoKeyPrefix().size()).toInt(&ok);
    return ok ? idx : -1;
}

//! Builds ~60 messages worth of 200 images total, with distinct per-message colours/senders.
inline std::vector<ImageRecord> buildDataset()
{
    std::vector<ImageRecord> images;
    images.reserve(220);

    std::vector<QString> senders={"Alice","Bob","Carol","Dave","Erin"};
    std::vector<std::pair<QColor,QColor>> palette=
    {
        {QColor("#4895ef"),QColor("#4361ee")},
        {QColor("#f72585"),QColor("#b5179e")},
        {QColor("#4cc9f0"),QColor("#4895ef")},
        {QColor("#f9c74f"),QColor("#f8961e")},
        {QColor("#90be6d"),QColor("#43aa8b")},
        {QColor("#f94144"),QColor("#f3722c")}
    };

    auto baseDateTime=QDateTime::currentDateTime().addDays(-3);
    int messageIndex=0;
    while (images.size()<200)
    {
        auto messageId=QString("msg-%1").arg(messageIndex);
        auto sender=senders[static_cast<size_t>(messageIndex)%senders.size()];
        auto colors=palette[static_cast<size_t>(messageIndex)%palette.size()];
        auto dt=baseDateTime.addSecs(messageIndex*240);

        // Message sizes cycle 1,2,3,4,1,2,3,4,... so both single-image (no album strip content
        // beyond the one item) and multi-image albums are well represented.
        auto imagesInMessage=1+(messageIndex%4);
        for (int i=0;i<imagesInMessage && images.size()<200;++i)
        {
            ImageRecord rec;
            rec.messageId=messageId;
            rec.sender=sender;
            rec.dateTime=dt;
            rec.c1=colors.first;
            rec.c2=colors.second;
            rec.label=QString("%1.%2").arg(messageIndex).arg(i+1);
            // First image of every 5th message animates -- scattered but not overwhelming across
            // the 200-image dataset (~12 images), enough to hit while paging/scrolling without
            // every album being animated.
            rec.isAnimated=(messageIndex%5==0 && i==0);
            images.push_back(rec);
        }
        ++messageIndex;
    }

    return images;
}

//--------------------------------------------------------------------------

//! Stands in for a real version-ladder PixmapSource (e.g. files2's thumbnail/chat/normal rungs):
//! delivers a blurry rung synchronously, then a sharp rung after a simulated network delay,
//! toggling isLoading() around the wait -- see AbstractImageViewer/ImageViewer's overlay spinner
//! and PixmapSource::setPixmapLoading()'s own doc for what a real source is expected to do here.
class DemoFlyweightSource : public PixmapSource
{
    public:

        explicit DemoFlyweightSource(std::vector<ImageRecord> images)
            : m_images(std::move(images))
        {}

        void setLatencyMs(int ms) noexcept
        {
            m_latencyMs=ms;
        }

    protected:

        void doLoadPixmap(const PixmapKey& key) override
        {
            auto idx=indexFromKey(key);
            if (idx<0 || static_cast<size_t>(idx)>=m_images.size())
            {
                return;
            }
            const auto& rec=m_images[static_cast<size_t>(idx)];

            // Tier 1: instantly-available low rung (mirrors an embedded/inline thumbnail already
            // decoded from the message itself -- no wait at all).
            updatePixmap(key,makeBlurryImage(key.size(),rec.c1,rec.c2));

            if (m_latencyMs<=0)
            {
                // "No latency" setting: skip the loading indicator/timer entirely and deliver the
                // sharp rung synchronously too.
                updatePixmap(key,makeSampleImage(key.size(),rec.c1,rec.c2,rec.label));
                if (rec.isAnimated)
                {
                    updateAnimation(key,AnimationContent{animatedGifResourcePath()});
                }
                return;
            }

            setPixmapLoading(key,true);

            // shared_from_this(), not a raw this-capture: PixmapSource is not a QObject, so
            // nothing else keeps it alive across the delay -- the demo's own std::shared_ptr
            // (held by the viewer's consumers) could otherwise be the last reference and this
            // callback would run against a dangling object.
            auto self=std::static_pointer_cast<DemoFlyweightSource>(shared_from_this());
            auto delay=m_latencyMs+static_cast<int>(QRandomGenerator::global()->bounded(400));
            QTimer::singleShot(
                delay,
                [self,key,rec]()
                {
                    // Tier 2: the sharp, full rung. updatePixmap() itself no-ops harmlessly if the
                    // producer was released (entry scrolled out of the active window) in the
                    // meantime -- see PixmapSource::updatePixmap()'s own key lookup.
                    self->updatePixmap(key,makeSampleImage(key.size(),rec.c1,rec.c2,rec.label));
                    if (rec.isAnimated)
                    {
                        self->updateAnimation(key,AnimationContent{animatedGifResourcePath()});
                    }
                    self->setPixmapLoading(key,false);
                }
            );
        }

    private:

        std::vector<ImageRecord> m_images;
        int m_latencyMs=700;
};

//--------------------------------------------------------------------------

//! Stands in for a paged chat-history fetch: given an anchor/count/direction from
//! AbstractImageViewer::imagesRequested(), looks up the next slice of the dataset and replies via
//! ChatImageViewer::insertFetchedChatImages() after a fixed simulated round-trip -- unless
//! m_failNextFetch is set, in which case this one request is silently dropped (see the "Fail next
//! fetch" checkbox), exercising AbstractImageViewer's pending-navigation timeout/recovery path.
class DemoChatFetcher : public QObject
{
    Q_OBJECT

    public:

        DemoChatFetcher(ChatImageViewer* viewer, std::vector<ImageRecord> images, QObject* parent=nullptr)
            : QObject(parent),
              m_viewer(viewer),
              m_images(std::move(images))
        {}

        void setFailNextFetch(bool enable) noexcept
        {
            m_failNextFetch=enable;
        }

    public slots:

        void onImagesRequested(const UISE_DESKTOP_NAMESPACE::PixmapKey& anchor, size_t maxCount, UISE_DESKTOP_NAMESPACE::Direction direction)
        {
            if (m_failNextFetch)
            {
                m_failNextFetch=false;
                return;
            }

            auto anchorIndex=indexFromKey(anchor);
            std::vector<int> indices;

            if (direction==Direction::END)
            {
                auto start=(anchorIndex<0) ? 0 : (anchorIndex+1);
                for (int i=start; i<static_cast<int>(m_images.size()) && indices.size()<maxCount; ++i)
                {
                    indices.push_back(i);
                }
            }
            else if (direction==Direction::HOME)
            {
                auto end=(anchorIndex<0) ? static_cast<int>(m_images.size())-1 : (anchorIndex-1);
                std::deque<int> collected;
                for (int i=end; i>=0 && collected.size()<maxCount; --i)
                {
                    collected.push_front(i);
                }
                indices.assign(collected.begin(),collected.end());
            }

            QPointer<ChatImageViewer> viewerGuard(m_viewer);
            auto images=m_images;
            QTimer::singleShot(
                400,
                [viewerGuard,images,indices,direction,maxCount]()
                {
                    if (!viewerGuard)
                    {
                        return;
                    }
                    std::vector<ChatImageViewer::ChatImage> page;
                    page.reserve(indices.size());
                    for (auto idx : indices)
                    {
                        const auto& rec=images[static_cast<size_t>(idx)];
                        // Content left null -- the viewer has a PixmapSource, so every image
                        // resolves (and upgrades) through it, never through a seed (see
                        // ChatImageViewer::makePreview()'s and D4's own doc on that precedence).
                        page.emplace_back(keyForIndex(idx),QPixmap{},rec.sender,rec.dateTime,rec.messageId);
                    }
                    viewerGuard->insertFetchedChatImages(std::move(page),direction,maxCount);
                }
            );
        }

    private:

        QPointer<ChatImageViewer> m_viewer;
        std::vector<ImageRecord> m_images;
        bool m_failNextFetch=false;
};

#endif // UISE_DEMO_CHATIMAGEVIEWERFLYWEIGHT_BACKEND_HPP
