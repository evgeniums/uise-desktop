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

/** @file uise/test/imagelabel/testimagelabel.cpp
*
*  Test ImageLabel.
*
*/

/****************************************************************************/

#include <QImage>
#include <QImageWriter>
#include <QBuffer>

#include <uise/test/uise-testthread.hpp>
#include <uise/test/uise-testutils.hpp>

#include <uise/desktop/imagelabel.hpp>

using namespace UISE_DESKTOP_NAMESPACE;
using namespace UISE_TEST_NAMESPACE;

using ImageLabelContainer=TestWidgetContainer<ImageLabel>;
using ImageLabelContainerPtr=std::shared_ptr<ImageLabelContainer>;

namespace {

// A hand-rolled 4x4, 2-frame, red/blue GIF89a with a NETSCAPE2.0 loop extension (89 bytes),
// generated once by a small script that writes the GIF block structure directly (no imaging
// library involved) and verified by re-parsing it back into exactly 2 image blocks. Kept tiny so
// it can live inline as a fixture instead of a binary test asset.
const unsigned char MiniAnimatedGif[]={
    0x47,0x49,0x46,0x38,0x39,0x61,0x04,0x00,0x04,0x00,0x80,0x00,0x00,0xff,0x00,0x00,
    0x00,0x00,0xff,0x21,0xff,0x0b,0x4e,0x45,0x54,0x53,0x43,0x41,0x50,0x45,0x32,0x2e,
    0x30,0x03,0x01,0x00,0x00,0x00,0x21,0xf9,0x04,0x00,0x0a,0x00,0x00,0x00,0x2c,0x00,
    0x00,0x00,0x00,0x04,0x00,0x04,0x00,0x00,0x02,0x04,0x84,0x8f,0x09,0x05,0x00,0x21,
    0xf9,0x04,0x00,0x0a,0x00,0x00,0x00,0x2c,0x00,0x00,0x00,0x00,0x04,0x00,0x04,0x00,
    0x00,0x02,0x04,0x8c,0x8f,0x19,0x05,0x00,0x3b,
};
const int MiniAnimatedGifSize=89;

QByteArray animatedGifBytes()
{
    return QByteArray(reinterpret_cast<const char*>(MiniAnimatedGif),MiniAnimatedGifSize);
}

QByteArray stillPngBytes()
{
    QImage image(6,6,QImage::Format_ARGB32);
    image.fill(Qt::red);

    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    QImageWriter writer(&buffer,"PNG");
    writer.write(image);
    return bytes;
}

}

BOOST_AUTO_TEST_SUITE(TestImageLabel)

BOOST_AUTO_TEST_CASE(TestDetection)
{
    auto init=[](ImageLabelContainerPtr container){
        ImageLabelContainer::PlayStepPeriod=300;
        auto img=new ImageLabel();
        ImageLabelContainer::beginTestCase(container,img,"Test ImageLabel detection");
    };

    auto loadStill=[](ImageLabelContainerPtr container){
        auto img=container->testWidget;
        UISE_TEST_CHECK(img->setImageData(stillPngBytes(),"png"));
    };

    auto checkStillThenLoadAnimated=[](ImageLabelContainerPtr container){
        auto img=container->testWidget;

        UISE_TEST_CHECK(!img->isAnimated());
        UISE_TEST_CHECK(img->frameCount()<=1);
        UISE_TEST_CHECK(!img->stillFrame().isNull());
        UISE_TEST_CHECK_EQUAL(img->naturalImageSize().width(),6);
        UISE_TEST_CHECK_EQUAL(img->naturalImageSize().height(),6);

        UISE_TEST_CHECK(img->setImageData(animatedGifBytes(),"gif"));
    };

    auto checkAnimated=[](ImageLabelContainerPtr container){
        auto img=container->testWidget;

        UISE_TEST_CHECK(img->isAnimated());
        // frameCount() may legitimately be 0 (unknown, not yet scanned) for some formats/timings,
        // but must never be exactly 1 -- that would mean the still-content demotion in
        // loadContent() disagreed with isAnimated() being true.
        UISE_TEST_CHECK(img->frameCount()==0 || img->frameCount()>1);
        UISE_TEST_CHECK_EQUAL(img->naturalImageSize().width(),4);
        UISE_TEST_CHECK_EQUAL(img->naturalImageSize().height(),4);
        UISE_TEST_CHECK(!img->stillFrame().isNull());
    };

    std::vector<std::function<void (ImageLabelContainerPtr container)>> steps={
        init,
        loadStill,
        checkStillThenLoadAnimated,
        checkAnimated
    };
    ImageLabelContainer::runTestCase(steps);
}

BOOST_AUTO_TEST_CASE(TestModeGating)
{
    auto init=[](ImageLabelContainerPtr container){
        ImageLabelContainer::PlayStepPeriod=300;
        auto img=new ImageLabel();
        img->setAnimationMode(ImageLabel::AnimationMode::Never);
        ImageLabelContainer::beginTestCase(container,img,"Test ImageLabel mode gating");
    };

    auto loadAnimated=[](ImageLabelContainerPtr container){
        auto img=container->testWidget;
        UISE_TEST_CHECK(img->setImageData(animatedGifBytes(),"gif"));
    };

    auto checkNeverThenSwitchToAuto=[](ImageLabelContainerPtr container){
        auto img=container->testWidget;

        UISE_TEST_CHECK(img->isAnimated());
        UISE_TEST_CHECK(!img->isPlaying());

        img->setAnimationMode(ImageLabel::AnimationMode::Auto);
    };

    auto checkAutoThenSwitchToNever=[](ImageLabelContainerPtr container){
        auto img=container->testWidget;

        // The container's window is shown by beginTestCase(), so Auto mode must be playing here.
        UISE_TEST_CHECK(img->isPlaying());

        img->setAnimationMode(ImageLabel::AnimationMode::Never);
    };

    auto checkNeverAgain=[](ImageLabelContainerPtr container){
        auto img=container->testWidget;
        UISE_TEST_CHECK(!img->isPlaying());
    };

    std::vector<std::function<void (ImageLabelContainerPtr container)>> steps={
        init,
        loadAnimated,
        checkNeverThenSwitchToAuto,
        checkAutoThenSwitchToNever,
        checkNeverAgain
    };
    ImageLabelContainer::runTestCase(steps);
}

BOOST_AUTO_TEST_CASE(TestManualControls)
{
    auto init=[](ImageLabelContainerPtr container){
        ImageLabelContainer::PlayStepPeriod=300;
        auto img=new ImageLabel();
        img->setAnimationMode(ImageLabel::AnimationMode::Manual);
        ImageLabelContainer::beginTestCase(container,img,"Test ImageLabel manual controls");
    };

    auto loadAnimated=[](ImageLabelContainerPtr container){
        auto img=container->testWidget;
        UISE_TEST_CHECK(img->setImageData(animatedGifBytes(),"gif"));
    };

    auto checkStoppedThenPlay=[](ImageLabelContainerPtr container){
        auto img=container->testWidget;

        UISE_TEST_CHECK(!img->isPlaying());
        img->play();
        UISE_TEST_CHECK(img->isPlaying());
    };

    auto checkPlayingThenPause=[](ImageLabelContainerPtr container){
        auto img=container->testWidget;

        UISE_TEST_CHECK(img->isPlaying());
        img->pause();
        UISE_TEST_CHECK(!img->isPlaying());
    };

    auto checkPausedThenPlayAgain=[](ImageLabelContainerPtr container){
        auto img=container->testWidget;

        UISE_TEST_CHECK(!img->isPlaying());
        img->play();
        UISE_TEST_CHECK(img->isPlaying());
    };

    auto checkPlayingThenStop=[](ImageLabelContainerPtr container){
        auto img=container->testWidget;

        UISE_TEST_CHECK(img->isPlaying());
        img->stop();
        UISE_TEST_CHECK(!img->isPlaying());
    };

    std::vector<std::function<void (ImageLabelContainerPtr container)>> steps={
        init,
        loadAnimated,
        checkStoppedThenPlay,
        checkPlayingThenPause,
        checkPausedThenPlayAgain,
        checkPlayingThenStop
    };
    ImageLabelContainer::runTestCase(steps);
}

BOOST_AUTO_TEST_CASE(TestHover)
{
    auto init=[](ImageLabelContainerPtr container){
        ImageLabelContainer::PlayStepPeriod=300;
        auto img=new ImageLabel();
        img->setAnimationMode(ImageLabel::AnimationMode::OnHover);
        ImageLabelContainer::beginTestCase(container,img,"Test ImageLabel hover");
    };

    auto loadAnimated=[](ImageLabelContainerPtr container){
        auto img=container->testWidget;
        UISE_TEST_CHECK(img->setImageData(animatedGifBytes(),"gif"));
        UISE_TEST_CHECK(!img->isPlaying());
    };

    auto hoverIn=[](ImageLabelContainerPtr container){
        auto img=container->testWidget;

        // Playback for OnHover is re-evaluated inside paintEvent() (see the class documentation
        // on why setParentHovered() itself cannot drive it directly), so an explicit repaint()
        // is required here to observe the transition synchronously rather than waiting for the
        // next regular paint.
        img->setParentHovered(true);
        img->repaint();
        UISE_TEST_CHECK(img->isPlaying());
    };

    auto hoverOut=[](ImageLabelContainerPtr container){
        auto img=container->testWidget;

        img->setParentHovered(false);
        img->repaint();
        UISE_TEST_CHECK(!img->isPlaying());
    };

    std::vector<std::function<void (ImageLabelContainerPtr container)>> steps={
        init,
        loadAnimated,
        hoverIn,
        hoverOut
    };
    ImageLabelContainer::runTestCase(steps);
}

BOOST_AUTO_TEST_CASE(TestHideShow)
{
    auto init=[](ImageLabelContainerPtr container){
        ImageLabelContainer::PlayStepPeriod=300;
        auto img=new ImageLabel();
        img->setAnimationMode(ImageLabel::AnimationMode::Auto);
        ImageLabelContainer::beginTestCase(container,img,"Test ImageLabel hide/show");
    };

    auto loadAnimated=[](ImageLabelContainerPtr container){
        auto img=container->testWidget;
        UISE_TEST_CHECK(img->setImageData(animatedGifBytes(),"gif"));
        UISE_TEST_CHECK(img->isPlaying());
    };

    auto doHide=[](ImageLabelContainerPtr container){
        auto img=container->testWidget;

        img->hide();
        UISE_TEST_CHECK(!img->isPlaying());
    };

    auto doShow=[](ImageLabelContainerPtr container){
        auto img=container->testWidget;

        img->show();
        UISE_TEST_CHECK(img->isPlaying());
    };

    std::vector<std::function<void (ImageLabelContainerPtr container)>> steps={
        init,
        loadAnimated,
        doHide,
        doShow
    };
    ImageLabelContainer::runTestCase(steps);
}

BOOST_AUTO_TEST_CASE(TestContentSwitch)
{
    int animatedChangedCount=0;

    auto init=[&animatedChangedCount](ImageLabelContainerPtr container){
        ImageLabelContainer::PlayStepPeriod=300;
        auto img=new ImageLabel();
        ImageLabelContainer::beginTestCase(container,img,"Test ImageLabel content switch");

        QObject::connect(img,&ImageLabel::animatedChanged,[&animatedChangedCount](bool){
            animatedChangedCount++;
        });
    };

    auto loadAnimated=[](ImageLabelContainerPtr container){
        auto img=container->testWidget;
        UISE_TEST_CHECK(img->setImageData(animatedGifBytes(),"gif"));
    };

    auto checkAnimatedThenLoadStill=[&animatedChangedCount](ImageLabelContainerPtr container){
        auto img=container->testWidget;

        UISE_TEST_CHECK(img->isAnimated());
        UISE_TEST_CHECK(img->pixmap().isNull());
        UISE_TEST_CHECK_EQUAL(animatedChangedCount,1);

        UISE_TEST_CHECK(img->setImageData(stillPngBytes(),"png"));
    };

    auto checkStillThenLoadAnimatedAgain=[&animatedChangedCount](ImageLabelContainerPtr container){
        auto img=container->testWidget;

        UISE_TEST_CHECK(!img->isAnimated());
        UISE_TEST_CHECK(!img->pixmap().isNull());
        UISE_TEST_CHECK_EQUAL(animatedChangedCount,2);

        UISE_TEST_CHECK(img->setImageData(animatedGifBytes(),"gif"));
    };

    auto checkAnimatedAgainThenClear=[&animatedChangedCount](ImageLabelContainerPtr container){
        auto img=container->testWidget;

        UISE_TEST_CHECK(img->isAnimated());
        UISE_TEST_CHECK_EQUAL(animatedChangedCount,3);

        img->clearImage();
    };

    auto checkCleared=[](ImageLabelContainerPtr container){
        auto img=container->testWidget;

        UISE_TEST_CHECK(!img->isAnimated());
        UISE_TEST_CHECK(!img->isPlaying());
        UISE_TEST_CHECK(img->pixmap().isNull());
        UISE_TEST_CHECK(img->stillFrame().isNull());
    };

    std::vector<std::function<void (ImageLabelContainerPtr container)>> steps={
        init,
        loadAnimated,
        checkAnimatedThenLoadStill,
        checkStillThenLoadAnimatedAgain,
        checkAnimatedAgainThenClear,
        checkCleared
    };
    ImageLabelContainer::runTestCase(steps);
}

BOOST_AUTO_TEST_CASE(TestTeardown)
{
    // Regression guard for the QMovie/QBuffer lifetime ordering documented in imagelabel.hpp:
    // deleting the label while its animation is actively playing must not crash. The actual
    // deletion happens via ImageLabelContainer::destroy() at the end of runStep(), through the
    // normal Qt parent-child chain (mainWindow -> content -> this widget) rather than an explicit
    // delete here -- that exercises the same ~ImageLabel() path a real host application relies on.
    auto init=[](ImageLabelContainerPtr container){
        ImageLabelContainer::PlayStepPeriod=300;
        auto img=new ImageLabel();
        img->setAnimationMode(ImageLabel::AnimationMode::Auto);
        img->setImageData(animatedGifBytes(),"gif");
        ImageLabelContainer::beginTestCase(container,img,"Test ImageLabel teardown");
    };

    auto checkPlaying=[](ImageLabelContainerPtr container){
        auto img=container->testWidget;
        UISE_TEST_CHECK(img->isPlaying());
    };

    std::vector<std::function<void (ImageLabelContainerPtr container)>> steps={
        init,
        checkPlaying
    };
    ImageLabelContainer::runTestCase(steps);
}

BOOST_AUTO_TEST_SUITE_END()
