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

/** @file demo/typingindicator/main.cpp
*
*  Demo application of TypingIndicator – animated triple-dots + text label.
*
*/

/****************************************************************************/

#include <QApplication>
#include <QMainWindow>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QGroupBox>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/style.hpp>
#include <uise/desktop/typingindicator.hpp>

using namespace UISE_DESKTOP_NAMESPACE;

//--------------------------------------------------------------------------

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // Load the bundled QSS (includes our light/dark typingindicator.qss).
    Style::instance().applyStyleSheet();

    // Extra base QSS that styles the demo chrome for each theme.
    const QString darkChrome  = "QFrame#mainFrame { background-color: #1a1a1a; }"
                                "QGroupBox { color: #cccccc; border: 1px solid #555; margin-top: 6px; }"
                                "QGroupBox::title { subcontrol-origin: margin; left: 8px; }";
    const QString lightChrome = "QFrame#mainFrame { background-color: #f0f0f0; }"
                                "QGroupBox { color: #222222; border: 1px solid #aaa; margin-top: 6px; }"
                                "QGroupBox::title { subcontrol-origin: margin; left: 8px; }";

    QMainWindow w;
    auto mainFrame = new QFrame();
    mainFrame->setObjectName("mainFrame");
    auto ml = Layout::vertical(mainFrame, false);

    // ---- preview area ----
    auto previewFrame = new QFrame();
    previewFrame->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    ml->addWidget(previewFrame, 1);
    auto pfl = Layout::horizontal(previewFrame, false);
    pfl->setAlignment(Qt::AlignCenter);

    auto indicator = new TypingIndicator(previewFrame);
    indicator->setText("typing");
    pfl->addWidget(indicator, 0, Qt::AlignCenter);

    // ---- controls area ----
    auto controlsFrame = new QFrame();
    ml->addWidget(controlsFrame, 0);
    auto cl = Layout::horizontal(controlsFrame, false);
    cl->setAlignment(Qt::AlignLeft);

    // -- text input --
    auto textGroup = new QGroupBox("Text");
    auto tl = Layout::horizontal(textGroup, false);
    auto textEdit = new QLineEdit("User is typing");
    textEdit->setMinimumWidth(160);
    tl->addWidget(textEdit);
    QObject::connect(textEdit, &QLineEdit::textChanged, indicator, &TypingIndicator::setText);
    cl->addWidget(textGroup);

    // -- dots position --
    auto posGroup = new QGroupBox("Dots position");
    auto posl = Layout::horizontal(posGroup, false);
    auto posCombo = new QComboBox();
    posCombo->addItems({"Left", "Right"});
    posl->addWidget(posCombo);
    QObject::connect(posCombo, &QComboBox::currentIndexChanged, indicator,
        [indicator](int idx)
        {
            indicator->setDotsPosition(
                idx == 0 ? TypingIndicator::DotsPosition::Left
                         : TypingIndicator::DotsPosition::Right);
        });
    cl->addWidget(posGroup);

    // -- dot radius --
    auto radiusGroup = new QGroupBox("Dot radius");
    auto rl = Layout::horizontal(radiusGroup, false);
    auto radiusSpin = new QSpinBox();
    radiusSpin->setRange(1, 20);
    radiusSpin->setValue(indicator->dotRadius());
    rl->addWidget(radiusSpin);
    QObject::connect(radiusSpin, &QSpinBox::valueChanged, indicator, &TypingIndicator::setDotRadius);
    cl->addWidget(radiusGroup);

    // -- dot spacing --
    auto spacingGroup = new QGroupBox("Dot spacing");
    auto spl = Layout::horizontal(spacingGroup, false);
    auto spacingSpin = new QSpinBox();
    spacingSpin->setRange(0, 40);
    spacingSpin->setValue(indicator->dotSpacing());
    spl->addWidget(spacingSpin);
    QObject::connect(spacingSpin, &QSpinBox::valueChanged, indicator, &TypingIndicator::setDotSpacing);
    cl->addWidget(spacingGroup);

    // -- active scale --
    auto scaleGroup = new QGroupBox("Active scale");
    auto scl = Layout::horizontal(scaleGroup, false);
    auto scaleSpin = new QDoubleSpinBox();
    scaleSpin->setRange(1.0, 4.0);
    scaleSpin->setSingleStep(0.1);
    scaleSpin->setValue(indicator->activeScale());
    scl->addWidget(scaleSpin);
    QObject::connect(scaleSpin, &QDoubleSpinBox::valueChanged, indicator, &TypingIndicator::setActiveScale);
    cl->addWidget(scaleGroup);

    // -- duration --
    auto durGroup = new QGroupBox("Duration (ms)");
    auto dl = Layout::horizontal(durGroup, false);
    auto durSpin = new QSpinBox();
    durSpin->setRange(200, 5000);
    durSpin->setSingleStep(100);
    durSpin->setValue(indicator->animationDurationMs());
    dl->addWidget(durSpin);
    QObject::connect(durSpin, &QSpinBox::valueChanged, indicator, &TypingIndicator::setAnimationDurationMs);
    cl->addWidget(durGroup);

    // -- start / stop --
    auto animGroup = new QGroupBox("Animation");
    auto al = Layout::horizontal(animGroup, false);
    auto startStop = new QPushButton("Stop");
    startStop->setCheckable(true);
    startStop->setChecked(true);
    al->addWidget(startStop);
    QObject::connect(startStop, &QPushButton::toggled, indicator,
        [indicator, startStop](bool running)
        {
            if (running)
            {
                startStop->setText("Stop");
                indicator->start();
            }
            else
            {
                startStop->setText("Start");
                indicator->stop();
            }
        });
    cl->addWidget(animGroup);

    // -- theme toggle --
    auto themeGroup = new QGroupBox("Theme");
    auto thl = Layout::horizontal(themeGroup, false);
    auto themeButton = new QPushButton();
    themeButton->setCheckable(true);
    const bool startDark = Style::instance().checkDarkTheme();
    themeButton->setChecked(startDark);
    themeButton->setText(startDark ? "Dark" : "Light");
    QObject::connect(themeButton, &QPushButton::toggled, mainFrame,
        [&darkChrome, &lightChrome, mainFrame, themeButton](bool dark)
        {
            if (dark)
            {
                Style::instance().setBaseQss(darkChrome);
                Style::instance().setColorTheme(Style::DarkTheme);
                themeButton->setText("Dark");
            }
            else
            {
                Style::instance().setBaseQss(lightChrome);
                Style::instance().setColorTheme(Style::LightTheme);
                themeButton->setText("Light");
            }
            Style::instance().applyStyleSheet(true);
            mainFrame->style()->unpolish(mainFrame);
            mainFrame->style()->polish(mainFrame);
        });
    thl->addWidget(themeButton);
    cl->addWidget(themeGroup);

    // Apply initial chrome QSS and start the animation.
    Style::instance().setBaseQss(startDark ? darkChrome : lightChrome);
    Style::instance().applyStyleSheet(true);
    indicator->start();

    w.setCentralWidget(mainFrame);
    w.resize(800, 300);
    w.setWindowTitle("TypingIndicator Demo");
    w.show();
    return app.exec();
}

//--------------------------------------------------------------------------
