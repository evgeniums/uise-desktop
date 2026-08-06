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

/** @file demo/imagelabel/main.cpp
*
*  Demo application of ImageLabel.
*
*/

/****************************************************************************/

#include <QApplication>
#include <QMainWindow>
#include <QScrollArea>
#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QSlider>
#include <QCheckBox>
#include <QFileDialog>
#include <QFile>
#include <QEvent>
#include <QEnterEvent>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/style.hpp>
#include <uise/desktop/imagelabel.hpp>

using namespace UISE_DESKTOP_NAMESPACE;

namespace {

const int TileSize=120;

QString sizeStyle(int size)
{
    return QString("QLabel{min-width:%1px;max-width:%1px;min-height:%1px;max-height:%1px;}").arg(size);
}

const char* AnimatedAsset=":/uise/desktop/demo/imagelabel/assets/animated.gif";
const char* StillAsset=":/uise/desktop/demo/imagelabel/assets/still.png";

struct ModeEntry
{
    ImageLabel::AnimationMode mode;
    const char* name;
};

const ModeEntry Modes[]={
    {ImageLabel::AnimationMode::Auto,"Auto"},
    {ImageLabel::AnimationMode::Never,"Never"},
    {ImageLabel::AnimationMode::OnHover,"OnHover"},
    {ImageLabel::AnimationMode::Manual,"Manual"}
};

// Loads via setImageFile(); the playground's "Load as data..." button below separately exercises
// the setImageData() path so both content-loading entry points are covered by the demo.
ImageLabel* makeTile(QWidget* parent, ImageLabel::AnimationMode mode, const QString& asset)
{
    auto img=new ImageLabel(parent);
    img->setStyleSheet(sizeStyle(TileSize));
    img->setCornersRadius(12,12);
    img->setAnimationMode(mode);
    img->setClickable(true);

    if (QFile::exists(asset))
    {
        img->setImageFile(asset);
    }

    return img;
}

//! Thin frame demonstrating the setParentHovered() path -- forwards its own hover state to the
//! ImageLabel it wraps, exercising ImageLabel::AnimationMode::OnHover driven by a parent widget
//! rather than the label's own enterEvent()/leaveEvent().
class HoverForwardFrame : public QFrame
{
    public:

        explicit HoverForwardFrame(ImageLabel* target, QWidget* parent=nullptr)
            : QFrame(parent),
              m_target(target)
        {
            setAttribute(Qt::WA_Hover);
        }

    protected:

        void enterEvent(QEnterEvent* event) override
        {
            QFrame::enterEvent(event);
            m_target->setParentHovered(true);
        }

        void leaveEvent(QEvent* event) override
        {
            QFrame::leaveEvent(event);
            m_target->setParentHovered(false);
        }

    private:

        ImageLabel* m_target;
};

}

//--------------------------------------------------------------------------

int main(int argc, char *argv[])
{
    QApplication app(argc,argv);

    Style::instance().applyStyleSheet();

    QMainWindow w;
    auto scrollArea=new QScrollArea();
    scrollArea->setWidgetResizable(true);

    auto mainFrame=new QFrame();
    auto l=Layout::vertical(mainFrame);
    scrollArea->setWidget(mainFrame);

    // --- row 1: one ImageLabel per mode, animated content ---
    l->addWidget(new QLabel("Animated content, one AnimationMode per column:"));
    auto row1=new QFrame(mainFrame);
    auto row1Layout=Layout::horizontal(row1);
    for (int i=0;i<4;i++)
    {
        auto col=new QFrame(row1);
        auto colLayout=Layout::vertical(col);
        auto img=makeTile(col,Modes[i].mode,AnimatedAsset);
        colLayout->addWidget(img);
        colLayout->addWidget(new QLabel(Modes[i].name,col));
        row1Layout->addWidget(col);
    }
    row1Layout->addStretch(1);
    l->addWidget(row1);

    // --- row 2: same four modes, still content -- must look identical regardless of mode ---
    l->addWidget(new QLabel("Still content, same four modes (must be inert):"));
    auto row2=new QFrame(mainFrame);
    auto row2Layout=Layout::horizontal(row2);
    for (int i=0;i<4;i++)
    {
        auto col=new QFrame(row2);
        auto colLayout=Layout::vertical(col);
        auto img=makeTile(col,Modes[i].mode,StillAsset);
        colLayout->addWidget(img);
        colLayout->addWidget(new QLabel(Modes[i].name,col));
        row2Layout->addWidget(col);
    }
    row2Layout->addStretch(1);
    l->addWidget(row2);

    // --- row 3: round OnHover label, plus one driven by a parent frame's hover state ---
    l->addWidget(new QLabel("Hover-driven playback: round label (direct hover) and framed label (setParentHovered()):"));
    auto row3=new QFrame(mainFrame);
    auto row3Layout=Layout::horizontal(row3);

    auto roundImg=makeTile(row3,ImageLabel::AnimationMode::OnHover,AnimatedAsset);
    roundImg->setAutoFitEllipse(true);
    row3Layout->addWidget(roundImg);

    auto framedImg=makeTile(row3,ImageLabel::AnimationMode::OnHover,AnimatedAsset);
    auto hoverFrame=new HoverForwardFrame(framedImg,row3);
    auto hoverFrameLayout=Layout::vertical(hoverFrame);
    hoverFrameLayout->addWidget(framedImg);
    hoverFrame->setStyleSheet("QFrame{border:1px dashed gray;}");
    row3Layout->addWidget(hoverFrame);

    row3Layout->addStretch(1);
    l->addWidget(row3);

    // --- control bar operating on a single "playground" label ---
    l->addWidget(new QLabel("Playground (controls below apply to this label):"));

    auto playground=new ImageLabel(mainFrame);
    playground->setStyleSheet(sizeStyle(200));
    playground->setCornersRadius(16,16);
    playground->setClickable(true);
    playground->setImageFile(AnimatedAsset);
    l->addWidget(playground);

    auto status=new QLabel("status: -",mainFrame);
    l->addWidget(status);

    QObject::connect(playground,&ImageLabel::playingChanged,[status](bool playing){
        status->setText(QString("playingChanged: %1").arg(playing?"true":"false"));
    });
    QObject::connect(playground,&ImageLabel::animatedChanged,[status](bool animated){
        status->setText(QString("animatedChanged: %1").arg(animated?"true":"false"));
    });
    QObject::connect(playground,&ImageLabel::imageLoaded,[status](){
        status->setText("imageLoaded");
    });
    QObject::connect(playground,&ImageLabel::imageLoadFailed,[status](const QString& err){
        status->setText(QString("imageLoadFailed: %1").arg(err));
    });
    QObject::connect(playground,&ImageLabel::animationFinished,[status](){
        status->setText("animationFinished");
    });
    QObject::connect(playground,&ImageLabel::clicked,[status](){
        status->setText("clicked");
    });

    auto controlBar1=new QFrame(mainFrame);
    auto cb1=Layout::horizontal(controlBar1);

    auto modeCombo=new QComboBox(controlBar1);
    for (const auto& m : Modes)
    {
        modeCombo->addItem(m.name);
    }
    QObject::connect(modeCombo,&QComboBox::currentIndexChanged,[playground](int index){
        playground->setAnimationMode(Modes[index].mode);
    });
    cb1->addWidget(new QLabel("Mode:",controlBar1));
    cb1->addWidget(modeCombo);

    auto playBtn=new QPushButton("Play",controlBar1);
    QObject::connect(playBtn,&QPushButton::clicked,playground,&ImageLabel::play);
    cb1->addWidget(playBtn);

    auto pauseBtn=new QPushButton("Pause",controlBar1);
    QObject::connect(pauseBtn,&QPushButton::clicked,playground,&ImageLabel::pause);
    cb1->addWidget(pauseBtn);

    auto stopBtn=new QPushButton("Stop",controlBar1);
    QObject::connect(stopBtn,&QPushButton::clicked,playground,&ImageLabel::stop);
    cb1->addWidget(stopBtn);

    auto toggleBtn=new QPushButton("Toggle",controlBar1);
    QObject::connect(toggleBtn,&QPushButton::clicked,playground,&ImageLabel::togglePlay);
    cb1->addWidget(toggleBtn);

    cb1->addStretch(1);
    l->addWidget(controlBar1);

    auto controlBar2=new QFrame(mainFrame);
    auto cb2=Layout::horizontal(controlBar2);

    auto openBtn=new QPushButton("Open file...",controlBar2);
    QObject::connect(openBtn,&QPushButton::clicked,[&w,playground](){
        auto fileName=QFileDialog::getOpenFileName(&w,"Open image","","Images (*.gif *.webp *.png *.jpg *.jpeg)");
        if (!fileName.isEmpty())
        {
            playground->setImageFile(fileName);
        }
    });
    cb2->addWidget(openBtn);

    auto loadDataBtn=new QPushButton("Load as data...",controlBar2);
    QObject::connect(loadDataBtn,&QPushButton::clicked,[&w,playground](){
        auto fileName=QFileDialog::getOpenFileName(&w,"Open image as bytes","","Images (*.gif *.webp *.png *.jpg *.jpeg)");
        if (!fileName.isEmpty())
        {
            QFile f(fileName);
            if (f.open(QIODevice::ReadOnly))
            {
                playground->setImageData(f.readAll());
            }
        }
    });
    cb2->addWidget(loadDataBtn);

    auto toAnimatedBtn=new QPushButton("Switch to animated",controlBar2);
    QObject::connect(toAnimatedBtn,&QPushButton::clicked,[playground](){
        playground->setImageFile(AnimatedAsset);
    });
    cb2->addWidget(toAnimatedBtn);

    auto toStillBtn=new QPushButton("Switch to still",controlBar2);
    QObject::connect(toStillBtn,&QPushButton::clicked,[playground](){
        playground->setImageFile(StillAsset);
    });
    cb2->addWidget(toStillBtn);

    auto clearBtn=new QPushButton("Clear",controlBar2);
    QObject::connect(clearBtn,&QPushButton::clicked,playground,&ImageLabel::clearImage);
    cb2->addWidget(clearBtn);

    cb2->addStretch(1);
    l->addWidget(controlBar2);

    auto controlBar3=new QFrame(mainFrame);
    auto cb3=Layout::horizontal(controlBar3);

    cb3->addWidget(new QLabel("Speed:",controlBar3));
    auto speedSlider=new QSlider(Qt::Horizontal,controlBar3);
    speedSlider->setRange(10,400);
    speedSlider->setValue(100);
    speedSlider->setFixedWidth(150);
    QObject::connect(speedSlider,&QSlider::valueChanged,playground,&ImageLabel::setAnimationSpeed);
    cb3->addWidget(speedSlider);

    auto hideShowBtn=new QPushButton("Hide",controlBar3);
    QObject::connect(hideShowBtn,&QPushButton::clicked,[playground,hideShowBtn](){
        if (playground->isVisible())
        {
            playground->hide();
            hideShowBtn->setText("Show");
        }
        else
        {
            playground->show();
            hideShowBtn->setText("Hide");
        }
    });
    cb3->addWidget(hideShowBtn);

    auto cacheCheck=new QCheckBox("Cache frames",controlBar3);
    QObject::connect(cacheCheck,&QCheckBox::toggled,playground,&ImageLabel::setCacheFrames);
    cb3->addWidget(cacheCheck);

    auto pauseInactiveCheck=new QCheckBox("Pause when window inactive",controlBar3);
    QObject::connect(pauseInactiveCheck,&QCheckBox::toggled,playground,&ImageLabel::setPauseWhenWindowInactive);
    cb3->addWidget(pauseInactiveCheck);

    cb3->addStretch(1);
    l->addWidget(controlBar3);

    // --- a label placed inside a small fixed-height scroll area, so it can be scrolled out of
    //     view to check that ImageLabel drops repaint work once fully clipped ---
    l->addWidget(new QLabel("Scroll the label below out of view to check the clipped-repaint CPU saving:"));
    auto clipArea=new QScrollArea(mainFrame);
    clipArea->setFixedHeight(80);
    clipArea->setWidgetResizable(true);
    auto clipContent=new QFrame();
    auto clipLayout=Layout::vertical(clipContent);
    clipLayout->addWidget(new QLabel("(scroll down)"));
    clipLayout->addSpacing(300);
    auto clippedImg=makeTile(clipContent,ImageLabel::AnimationMode::Auto,AnimatedAsset);
    clipLayout->addWidget(clippedImg);
    clipLayout->addStretch(1);
    clipArea->setWidget(clipContent);
    l->addWidget(clipArea);

    l->addStretch(1);

    w.setCentralWidget(scrollArea);
    w.resize(900,900);
    w.setWindowTitle("ImageLabel Demo");
    w.show();

    auto ret=app.exec();
    return ret;
}

//--------------------------------------------------------------------------
