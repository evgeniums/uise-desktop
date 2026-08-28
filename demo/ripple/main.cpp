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

/** @file demo/ripple/main.cpp
*
*  Demo application of RippleOverlay, on both IconTextButton and Calendar day cells.
*
*/

/****************************************************************************/

#include <vector>

#include <QApplication>
#include <QMainWindow>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QGroupBox>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/style.hpp>
#include <uise/desktop/icontextbutton.hpp>
#include <uise/desktop/calendar.hpp>
#include <uise/desktop/ripple.hpp>

using namespace UISE_DESKTOP_NAMESPACE;

//--------------------------------------------------------------------------

//! Every RippleOverlay currently on screen -- IconTextButtons' own plus every non-null
//! CalendarDay cell's -- so the control panel below can retune all of them at once.
std::vector<RippleOverlay*> collectRipples(const std::vector<IconTextButton*>& buttons, Calendar* calendar)
{
    std::vector<RippleOverlay*> ripples;
    for (auto* button : buttons)
    {
        ripples.push_back(button->rippleOverlay());
    }
    for (int row=0; row<Calendar::GridRows; ++row)
    {
        for (int col=0; col<Calendar::GridColumns; ++col)
        {
            if (auto* day=calendar->dayCell(row,col))
            {
                ripples.push_back(day->rippleOverlay());
            }
        }
    }
    return ripples;
}

//--------------------------------------------------------------------------

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // Load the bundled QSS (includes our light/dark ripple.qss).
    Style::instance().applyStyleSheet();

    // Theme-independent: fixes the icon at a sane size regardless of the source SVG's own
    // dimensions, same rule the icontextbutton demo applies to its own buttons.
    //
    // The icon-only row (see the "iconOnly" dynamic property set below) is additionally styled
    // as a round button -- 9px padding around an 18px icon makes the content box exactly 18px
    // square on every side, so it fills a fixed 36px frame without needing a layout alignment
    // fix, and a 18px border-radius (half the frame size) turns the square into a circle. The
    // ripple itself needs no override here any more: IconTextButton now installs it directly on
    // the (always-square) icon rather than the whole button, so ripple.qss already gives every
    // IconTextButton a centred, true-circle ripple regardless of the button's own shape.
    //
    // The hover background-color rule further down is a rect with rounded corners in Qt's own
    // box model regardless of border-radius, which reads as visibly non-circular next to the
    // icon's true-circle ripple -- so hover feedback for icon-only buttons is left to the
    // icon's own hovered state (RoundedImage::setParentHovered(), driven automatically by
    // IconTextButton::setHovered()) instead of a background fill.
    const QString commonChrome = "uise--IconTextButton uise--RoundedImage {"
                                  "min-width:18px;max-width:18px;min-height:18px;max-height:18px;"
                                  "}"
                                  "uise--IconTextButton[iconOnly=\"true\"] {"
                                  "min-width:36px;max-width:36px;min-height:36px;max-height:36px;"
                                  "padding:9px;"
                                  "border-radius:18px;"
                                  "}"
                                  "uise--IconTextButton[iconOnly=\"true\"][hovered=\"true\"] {"
                                  "background-color: transparent;"
                                  "}"
                                  "uise--IconTextButton#wideRoundedRectDemo uise--RippleOverlay {"
                                  "qproperty-rippleShape: \"roundedrect\";"
                                  "qproperty-rippleRadiusScaleX: 1.0;"
                                  "qproperty-rippleRadiusScaleY: 1.0;"
                                  "}";

    const QString darkChrome  = commonChrome +
                                "QFrame#mainFrame { background-color: #1a1a1a; }"
                                "QGroupBox { color: #cccccc; border: 1px solid #555; margin-top: 6px; }"
                                "QGroupBox::title { subcontrol-origin: margin; left: 8px; }"
                                "uise--IconTextButton { border-radius: 4px; padding: 6px 10px; }"
                                "uise--IconTextButton QLabel { color: #eeeeee; }"
                                "uise--IconTextButton[hovered=\"true\"] { background-color: #333333; }";
    const QString lightChrome = commonChrome +
                                "QFrame#mainFrame { background-color: #f0f0f0; }"
                                "QGroupBox { color: #222222; border: 1px solid #aaa; margin-top: 6px; }"
                                "QGroupBox::title { subcontrol-origin: margin; left: 8px; }"
                                "uise--IconTextButton { border-radius: 4px; padding: 6px 10px; }"
                                "uise--IconTextButton[hovered=\"true\"] { background-color: #e0e0e0; }";

    QMainWindow w;
    auto mainFrame = new QFrame();
    mainFrame->setObjectName("mainFrame");
    auto ml = Layout::vertical(mainFrame, false);

    // ---- preview area: a row of buttons and a calendar ----
    auto previewFrame = new QFrame();
    previewFrame->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    ml->addWidget(previewFrame, 1);
    auto pfl = Layout::horizontal(previewFrame, false);
    pfl->setAlignment(Qt::AlignCenter);

    std::vector<IconTextButton*> buttons;
    auto buttonsColumn = new QFrame(previewFrame);
    auto bcl = Layout::vertical(buttonsColumn, false);
    for (const auto& text : {"Save", "Delete", "Send", "Cancel"})
    {
        auto button = new IconTextButton(QString(text), buttonsColumn);
        button->setMinimumWidth(140);
        bcl->addWidget(button);
        buttons.push_back(button);
    }

    // Icon-only buttons -- no text set, so IconTextButton's own text label stays hidden (see
    // IconTextButton::setText()) and only the ripple + icon show. The "iconOnly" dynamic
    // property drives the round-button + circular-ripple QSS above (see commonChrome).
    auto iconOnlyRow = new QFrame(buttonsColumn);
    auto iorl = Layout::horizontal(iconOnlyRow, false);
    for (const auto& alias : {"chats", "home", "lock", "quit"})
    {
        auto button = new IconTextButton(iconOnlyRow);
        button->setSvgIcon(Style::instance().svgIconLocator().icon(QString("FastSwitchButton::%1").arg(alias),button));
        button->setProperty("iconOnly",true);
        iorl->addWidget(button);
        buttons.push_back(button);
    }
    bcl->addWidget(iconOnlyRow);

    // Wide-row button with rippleShape:"roundedrect" -- demonstrates the shape NavigationBarItem
    // uses (see navigationbar.qss) for a host too wide/short for Ellipse's corner-based radius
    // to read as anything but a flattened band. objectName-scoped so only this one button gets
    // it, everything else keeps the default Ellipse.
    auto wideButton = new IconTextButton(QStringLiteral("Wide row (roundedrect)"), buttonsColumn);
    wideButton->setObjectName("wideRoundedRectDemo");
    wideButton->setMinimumWidth(220);
    bcl->addWidget(wideButton);
    buttons.push_back(wideButton);

    pfl->addWidget(buttonsColumn, 0, Qt::AlignCenter);

    auto calendar = new Calendar(CalendarMode::Activation, previewFrame);
    pfl->addWidget(calendar, 0, Qt::AlignCenter);

    // ---- controls area ----
    auto controlsFrame = new QFrame();
    ml->addWidget(controlsFrame, 0);
    auto cl = Layout::horizontal(controlsFrame, false);
    cl->setAlignment(Qt::AlignLeft);

    // -- enabled --
    auto enabledGroup = new QGroupBox("Ripple");
    auto el = Layout::horizontal(enabledGroup, false);
    auto enabledCheck = new QCheckBox("Enabled");
    enabledCheck->setChecked(true);
    el->addWidget(enabledCheck);
    QObject::connect(enabledCheck, &QCheckBox::toggled, &app,
        [buttons, calendar](bool enable)
        {
            for (auto* ripple : collectRipples(buttons, calendar))
            {
                ripple->setRippleEnabled(enable);
            }
        });
    cl->addWidget(enabledGroup);

    // -- hold on press --
    auto holdGroup = new QGroupBox("Hold on press");
    auto hl = Layout::horizontal(holdGroup, false);
    auto holdCheck = new QCheckBox("Hold");
    holdCheck->setChecked(true);
    hl->addWidget(holdCheck);
    QObject::connect(holdCheck, &QCheckBox::toggled, &app,
        [buttons, calendar](bool enable)
        {
            for (auto* ripple : collectRipples(buttons, calendar))
            {
                ripple->setRippleHoldOnPress(enable);
            }
        });
    cl->addWidget(holdGroup);

    // -- grow duration --
    auto durGroup = new QGroupBox("Grow duration (ms)");
    auto dl = Layout::horizontal(durGroup, false);
    auto durSpin = new QSpinBox();
    durSpin->setRange(50, 2000);
    durSpin->setSingleStep(50);
    durSpin->setValue(RippleOverlay::DefaultDurationMs);
    dl->addWidget(durSpin);
    QObject::connect(durSpin, &QSpinBox::valueChanged, &app,
        [buttons, calendar](int ms)
        {
            for (auto* ripple : collectRipples(buttons, calendar))
            {
                ripple->setRippleDurationMs(ms);
            }
        });
    cl->addWidget(durGroup);

    // -- fade duration --
    auto fadeGroup = new QGroupBox("Fade duration (ms)");
    auto fl = Layout::horizontal(fadeGroup, false);
    auto fadeSpin = new QSpinBox();
    fadeSpin->setRange(50, 2000);
    fadeSpin->setSingleStep(50);
    fadeSpin->setValue(RippleOverlay::DefaultFadeDurationMs);
    fl->addWidget(fadeSpin);
    QObject::connect(fadeSpin, &QSpinBox::valueChanged, &app,
        [buttons, calendar](int ms)
        {
            for (auto* ripple : collectRipples(buttons, calendar))
            {
                ripple->setRippleFadeDurationMs(ms);
            }
        });
    cl->addWidget(fadeGroup);

    // -- opacity --
    auto opGroup = new QGroupBox("Opacity");
    auto ol = Layout::horizontal(opGroup, false);
    auto opSpin = new QDoubleSpinBox();
    opSpin->setRange(0.0, 1.0);
    opSpin->setDecimals(3);
    opSpin->setSingleStep(0.01);
    opSpin->setValue(RippleOverlay::DefaultOpacity);
    ol->addWidget(opSpin);
    QObject::connect(opSpin, &QDoubleSpinBox::valueChanged, &app,
        [buttons, calendar](double opacity)
        {
            for (auto* ripple : collectRipples(buttons, calendar))
            {
                ripple->setRippleOpacity(opacity);
            }
        });
    cl->addWidget(opGroup);

    // -- calendar mode --
    auto modeGroup = new QGroupBox("Calendar mode");
    auto mdl = Layout::horizontal(modeGroup, false);
    auto modeCombo = new QComboBox();
    modeCombo->addItems({"Activation", "Single selection", "Range selection", "Multiple selection", "Auto"});
    modeCombo->setCurrentIndex(0);
    mdl->addWidget(modeCombo);
    QObject::connect(modeCombo, &QComboBox::currentIndexChanged, calendar,
        [calendar](int idx)
        {
            switch (idx)
            {
                case (1): calendar->setMode(CalendarMode::SingleSelection); break;
                case (2): calendar->setMode(CalendarMode::RangeSelection); break;
                case (3): calendar->setMode(CalendarMode::MultipleSelection); break;
                case (4): calendar->setMode(CalendarMode::Auto); break;
                default: calendar->setMode(CalendarMode::Activation); break;
            }
        });
    cl->addWidget(modeGroup);

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

    // Apply initial chrome QSS.
    Style::instance().setBaseQss(startDark ? darkChrome : lightChrome);
    Style::instance().applyStyleSheet(true);

    w.setCentralWidget(mainFrame);
    w.resize(700, 500);
    w.setWindowTitle("Ripple Demo");
    w.show();
    return app.exec();
}

//--------------------------------------------------------------------------
