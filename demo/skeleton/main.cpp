/**
@copyright Evgeny Sidorov 2022-2025

This software is dual-licensed. Choose the appropriate license for your project.

1. The GNU GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-GPLv3.md](LICENSE-GPLv3.md) or copy at https://www.gnu.org/licenses/gpl-3.0.txt)

2. The GNU LESSER GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-LGPLv3.md](LICENSE-LGPLv3.md) or copy at https://www.gnu.org/licenses/lgpl-3.0.txt).

You may select, at your option, one of the above-listed licenses.

*/

/****************************************************************************/

/** @file demo/skeleton/main.cpp
*
*  Demo application of Skeleton shimmer placeholder widget.
*
*/

/****************************************************************************/

#include <QApplication>
#include <QMainWindow>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QComboBox>
#include <QGroupBox>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/style.hpp>
#include <uise/desktop/skeleton.hpp>

using namespace UISE_DESKTOP_NAMESPACE;

//--------------------------------------------------------------------------

static void rebuildSkeleton(Skeleton* s, int preset, int lineCount, int lineSpinnerHeight)
{
    s->clearItems();

    switch (preset)
    {
        case 0: // Avatar + lines (default card)
            s->addCircle(48);
            for (int i=0;i<lineCount;i++)
            {
                s->newRow();
                double factor=1.0-i*0.12;
                if (factor<0.3) factor=0.3;
                s->addLine(factor,lineSpinnerHeight);
            }
            break;

        case 1: // Text-only lines
            for (int i=0;i<lineCount;i++)
            {
                if (i>0) s->newRow();
                double factor=1.0-i*0.08;
                if (factor<0.4) factor=0.4;
                s->addLine(factor,lineSpinnerHeight);
            }
            break;

        case 2: // Grid of blocks
            for (int row=0;row<lineCount;row++)
            {
                if (row>0) s->newRow();
                s->addBlock(80,60);
                s->addBlock(80,60);
                s->addBlock(80,60);
            }
            break;

        case 3: // Mixed row (circle + two blocks)
            s->addCircle(40);
            s->addBlock(120,40);
            s->newRow();
            s->addLine(0.6,lineSpinnerHeight);
            s->addLine(0.3,lineSpinnerHeight);
            break;

        default:
            break;
    }
}

//--------------------------------------------------------------------------

int main(int argc, char *argv[])
{
    QApplication app(argc,argv);

    QString darkTheme="QFrame#mainFrame { background-color: #1a1a1a; } QGroupBox { color: #cccccc; border: 1px solid #555; margin-top: 6px; } QGroupBox::title { subcontrol-origin: margin; left: 8px; }";
    QString lightTheme="QFrame#mainFrame { background-color: #f0f0f0; } QGroupBox { color: #222222; border: 1px solid #aaa; margin-top: 6px; } QGroupBox::title { subcontrol-origin: margin; left: 8px; }";

    Style::instance().applyStyleSheet();

    QMainWindow w;
    auto mainFrame=new QFrame();
    mainFrame->setObjectName("mainFrame");
    auto ml=Layout::vertical(mainFrame,false);

    // --- top: skeleton widget fills most of the window ---
    auto skeleton=new Skeleton(mainFrame);
    skeleton->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);
    ml->addWidget(skeleton,1);

    // --- bottom: controls ---
    auto controlsFrame=new QFrame();
    ml->addWidget(controlsFrame,0);
    auto cl=Layout::horizontal(controlsFrame,false);

    // Start / Stop
    auto startStopGroup=new QGroupBox("Animation");
    auto ssl=Layout::horizontal(startStopGroup,false);

    auto startStop=new QPushButton("Stop");
    startStop->setCheckable(true);
    startStop->setChecked(true);
    ssl->addWidget(startStop);
    QObject::connect(startStop,&QPushButton::toggled,skeleton,
        [skeleton,startStop](bool checked)
        {
            if (checked)
            {
                startStop->setText("Stop");
                skeleton->start();
            }
            else
            {
                startStop->setText("Start");
                skeleton->stop();
            }
        }
    );
    // start animation after controls are wired so button state stays in sync
    skeleton->start();
    cl->addWidget(startStopGroup);

    // Layout preset
    auto presetGroup=new QGroupBox("Preset");
    auto pl=Layout::horizontal(presetGroup,false);
    auto presetCombo=new QComboBox();
    presetCombo->addItems({"Avatar + lines","Text lines","Block grid","Mixed"});
    pl->addWidget(presetCombo);
    cl->addWidget(presetGroup);

    // Line count
    auto lineCountGroup=new QGroupBox("Lines / rows");
    auto lcl=Layout::horizontal(lineCountGroup,false);
    auto lineCount=new QSpinBox();
    lineCount->setRange(1,10);
    lineCount->setValue(3);
    lcl->addWidget(lineCount);
    cl->addWidget(lineCountGroup);

    // Line height
    auto lineHeightGroup=new QGroupBox("Line height");
    auto lhl=Layout::horizontal(lineHeightGroup,false);
    auto lineHeight=new QSpinBox();
    lineHeight->setRange(6,48);
    lineHeight->setValue(12);
    lhl->addWidget(lineHeight);
    cl->addWidget(lineHeightGroup);

    // Item spacing
    auto spacingGroup=new QGroupBox("Spacing");
    auto spl=Layout::horizontal(spacingGroup,false);
    auto spacing=new QSpinBox();
    spacing->setRange(0,32);
    spacing->setValue(skeleton->itemSpacing());
    spl->addWidget(spacing);
    cl->addWidget(spacingGroup);

    // Corner radius
    auto radiusGroup=new QGroupBox("Radius");
    auto rl=Layout::horizontal(radiusGroup,false);
    auto radius=new QSpinBox();
    radius->setRange(0,32);
    radius->setValue(skeleton->cornerRadius());
    rl->addWidget(radius);
    cl->addWidget(radiusGroup);

    // Shimmer duration
    auto durationGroup=new QGroupBox("Duration (ms)");
    auto dl=Layout::horizontal(durationGroup,false);
    auto duration=new QSpinBox();
    duration->setRange(200,5000);
    duration->setSingleStep(100);
    duration->setValue(skeleton->shimmerDurationMs());
    dl->addWidget(duration);
    cl->addWidget(durationGroup);

    // Theme toggle
    auto themeGroup=new QGroupBox("Theme");
    auto thl=Layout::horizontal(themeGroup,false);
    auto themeButton=new QPushButton();
    themeButton->setCheckable(true);
    if (Style::instance().checkDarkTheme())
    {
        themeButton->setChecked(true);
        themeButton->setText("Dark");
    }
    else
    {
        themeButton->setChecked(false);
        themeButton->setText("Light");
    }
    QObject::connect(themeButton,&QPushButton::toggled,mainFrame,
        [&darkTheme,&lightTheme,mainFrame,themeButton](bool dark)
        {
            if (dark)
            {
                Style::instance().setBaseQss(darkTheme);
                Style::instance().setColorTheme(Style::DarkTheme);
                themeButton->setText("Dark");
            }
            else
            {
                Style::instance().setBaseQss(lightTheme);
                Style::instance().setColorTheme(Style::LightTheme);
                themeButton->setText("Light");
            }
            Style::instance().applyStyleSheet(true);
            mainFrame->style()->unpolish(mainFrame);
            mainFrame->style()->polish(mainFrame);
        }
    );
    thl->addWidget(themeButton);
    cl->addWidget(themeGroup);

    // Wire rebuild on any layout parameter change
    auto rebuild=[skeleton,presetCombo,lineCount,lineHeight]()
    {
        rebuildSkeleton(skeleton,presetCombo->currentIndex(),
                        lineCount->value(),lineHeight->value());
    };

    QObject::connect(presetCombo,&QComboBox::currentIndexChanged,skeleton,rebuild);
    QObject::connect(lineCount,&QSpinBox::valueChanged,skeleton,[rebuild](int){rebuild();});
    QObject::connect(lineHeight,&QSpinBox::valueChanged,skeleton,[rebuild](int){rebuild();});

    QObject::connect(spacing,&QSpinBox::valueChanged,skeleton,
        [skeleton](int v){ skeleton->setItemSpacing(v); });
    QObject::connect(radius,&QSpinBox::valueChanged,skeleton,
        [skeleton](int v){ skeleton->setCornerRadius(v); });
    QObject::connect(duration,&QSpinBox::valueChanged,skeleton,
        [skeleton](int v){ skeleton->setShimmerDurationMs(v); });

    w.setCentralWidget(mainFrame);
    w.resize(700,500);
    w.setWindowTitle("Skeleton Shimmer Placeholder Demo");
    w.show();
    auto ret=app.exec();
    return ret;
}

//--------------------------------------------------------------------------
