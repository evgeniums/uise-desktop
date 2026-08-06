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

/** @file demo/checkbox/main.cpp
*
*  Demo application of CheckBox and RadioBox.
*
*/

/****************************************************************************/

#include <vector>

#include <QApplication>
#include <QMainWindow>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <QRadioButton>
#include <QButtonGroup>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QGroupBox>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/style.hpp>
#include <uise/desktop/abstractcheckbox.hpp>
#include <uise/desktop/checkbox.hpp>
#include <uise/desktop/radiobox.hpp>
#include <uise/desktop/ripple.hpp>

using namespace UISE_DESKTOP_NAMESPACE;

//--------------------------------------------------------------------------

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // Load the bundled QSS (includes our light/dark checkbox.qss and ripple.qss).
    Style::instance().applyStyleSheet();

    const QString commonChrome = "";

    const QString darkChrome  = commonChrome +
                                "QFrame#mainFrame { background-color: #1a1a1a; }"
                                "QGroupBox { color: #cccccc; border: 1px solid #555; margin-top: 6px; }"
                                "QGroupBox::title { subcontrol-origin: margin; left: 8px; }";
    const QString lightChrome = commonChrome +
                                "QFrame#mainFrame { background-color: #f0f0f0; }"
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
    pfl->setAlignment(Qt::AlignTop | Qt::AlignLeft);

    std::vector<AbstractCheckBox*> boxes;

    // -- CheckBox column: with/without text, disabled, long text --
    auto checkColumn = new QFrame(previewFrame);
    auto ccl = Layout::vertical(checkColumn, false);
    ccl->setSpacing(6);
    auto checkLabel = new QLabel("CheckBox");
    checkLabel->setObjectName("sectionLabel");
    ccl->addWidget(checkLabel);

    auto check1 = new CheckBox("Enable notifications", checkColumn);
    ccl->addWidget(check1);
    boxes.push_back(check1);

    auto check2 = new CheckBox(checkColumn);
    check2->setChecked(true);
    ccl->addWidget(check2);
    boxes.push_back(check2);

    auto check3 = new CheckBox("Disabled option", checkColumn);
    check3->setEnabled(false);
    ccl->addWidget(check3);
    boxes.push_back(check3);

    auto check4 = new CheckBox("A checkbox with a fairly long label to see how the text wraps or elides", checkColumn);
    ccl->addWidget(check4);
    boxes.push_back(check4);

    pfl->addWidget(checkColumn);

    // -- RadioBox column: a real QButtonGroup --
    auto radioColumn1 = new QFrame(previewFrame);
    auto rcl1 = Layout::vertical(radioColumn1, false);
    rcl1->setSpacing(6);
    auto radioLabel1 = new QLabel("RadioBox (QButtonGroup)");
    radioLabel1->setObjectName("sectionLabel");
    rcl1->addWidget(radioLabel1);

    auto radioGroup1 = new QButtonGroup(radioColumn1);
    for (const auto& text : {"Small", "Medium", "Large"})
    {
        auto radio = new RadioBox(QString(text), radioColumn1);
        rcl1->addWidget(radio);
        radioGroup1->addButton(radio);
        boxes.push_back(radio);
    }
    radioGroup1->buttons().first()->setChecked(true);

    pfl->addWidget(radioColumn1);

    // -- RadioBox column: exclusive purely via autoExclusive() + shared parent, no QButtonGroup --
    auto radioColumn2 = new QFrame(previewFrame);
    auto rcl2 = Layout::vertical(radioColumn2, false);
    rcl2->setSpacing(6);
    auto radioLabel2 = new QLabel("RadioBox (autoExclusive)");
    radioLabel2->setObjectName("sectionLabel");
    rcl2->addWidget(radioLabel2);

    RadioBox* firstAutoRadio=nullptr;
    for (const auto& text : {"Daily", "Weekly", "Monthly"})
    {
        auto radio = new RadioBox(QString(text), radioColumn2);
        rcl2->addWidget(radio);
        boxes.push_back(radio);
        if (firstAutoRadio==nullptr)
        {
            firstAutoRadio=radio;
        }
    }
    if (firstAutoRadio!=nullptr)
    {
        firstAutoRadio->setChecked(true);
    }

    pfl->addWidget(radioColumn2);

    // -- native Qt widgets, for a visual side-by-side diff --
    auto nativeColumn = new QFrame(previewFrame);
    auto ncl = Layout::vertical(nativeColumn, false);
    ncl->setSpacing(6);
    auto nativeLabel = new QLabel("Native (QCheckBox/QRadioButton)");
    nativeLabel->setObjectName("sectionLabel");
    ncl->addWidget(nativeLabel);
    auto nativeCheck = new QCheckBox("Native checkbox", nativeColumn);
    ncl->addWidget(nativeCheck);
    auto nativeRadioGroup = new QButtonGroup(nativeColumn);
    for (const auto& text : {"Option A", "Option B"})
    {
        auto radio = new QRadioButton(QString(text), nativeColumn);
        ncl->addWidget(radio);
        nativeRadioGroup->addButton(radio);
    }
    nativeRadioGroup->buttons().first()->setChecked(true);

    pfl->addWidget(nativeColumn);
    pfl->addStretch(1);

    // ---- controls area ----
    auto controlsFrame = new QFrame();
    ml->addWidget(controlsFrame, 0);
    auto cl = Layout::horizontal(controlsFrame, false);
    cl->setAlignment(Qt::AlignLeft);

    // -- check animation --
    auto animGroup = new QGroupBox("Check animation");
    auto al = Layout::horizontal(animGroup, false);
    auto animEnabledCheck = new QCheckBox("Enabled");
    animEnabledCheck->setChecked(true);
    al->addWidget(animEnabledCheck);
    auto animDurSpin = new QSpinBox();
    animDurSpin->setRange(0, 2000);
    animDurSpin->setSingleStep(20);
    animDurSpin->setValue(AbstractCheckBox::DefaultCheckAnimationDurationMs);
    al->addWidget(animDurSpin);
    auto easingCombo = new QComboBox();
    const std::vector<std::pair<QString,int>> easingCurves = {
        {"Linear", QEasingCurve::Linear},
        {"InQuad", QEasingCurve::InQuad},
        {"OutQuad", QEasingCurve::OutQuad},
        {"InOutQuad", QEasingCurve::InOutQuad},
        {"OutCubic", QEasingCurve::OutCubic},
        {"InOutCubic", QEasingCurve::InOutCubic},
        {"OutBounce", QEasingCurve::OutBounce},
        {"OutElastic", QEasingCurve::OutElastic}
    };
    for (const auto& item : easingCurves)
    {
        easingCombo->addItem(item.first, item.second);
    }
    easingCombo->setCurrentIndex(4); // OutCubic, the default
    al->addWidget(easingCombo);
    QObject::connect(animEnabledCheck, &QCheckBox::toggled, &app,
        [boxes](bool enable)
        {
            for (auto* box : boxes)
            {
                box->setCheckAnimationEnabled(enable);
            }
        });
    QObject::connect(animDurSpin, &QSpinBox::valueChanged, &app,
        [boxes](int ms)
        {
            for (auto* box : boxes)
            {
                box->setCheckAnimationDurationMs(ms);
            }
        });
    QObject::connect(easingCombo, &QComboBox::currentIndexChanged, &app,
        [boxes, easingCombo](int idx)
        {
            for (auto* box : boxes)
            {
                box->setCheckAnimationEasingCurveType(easingCombo->itemData(idx).toInt());
            }
        });
    cl->addWidget(animGroup);

    // -- indicator mode --
    auto modeGroup = new QGroupBox("Indicator mode");
    auto mgl = Layout::horizontal(modeGroup, false);
    auto modeCombo = new QComboBox();
    modeCombo->addItems({"Auto", "Svg", "Qss"});
    mgl->addWidget(modeCombo);
    QObject::connect(modeCombo, &QComboBox::currentIndexChanged, &app,
        [boxes](int idx)
        {
            AbstractCheckBox::IndicatorMode mode=AbstractCheckBox::IndicatorMode::Auto;
            if (idx==1)
            {
                mode=AbstractCheckBox::IndicatorMode::Svg;
            }
            else if (idx==2)
            {
                mode=AbstractCheckBox::IndicatorMode::Qss;
            }
            for (auto* box : boxes)
            {
                box->setIndicatorMode(mode);
            }
        });
    cl->addWidget(modeGroup);

    // -- text position --
    auto posGroup = new QGroupBox("Text position");
    auto pgl = Layout::horizontal(posGroup, false);
    auto posCombo = new QComboBox();
    posCombo->addItems({"After", "Before"});
    pgl->addWidget(posCombo);
    QObject::connect(posCombo, &QComboBox::currentIndexChanged, &app,
        [boxes](int idx)
        {
            auto pos=(idx==1) ? AbstractCheckBox::TextPosition::Before : AbstractCheckBox::TextPosition::After;
            for (auto* box : boxes)
            {
                box->setTextPosition(pos);
            }
        });
    cl->addWidget(posGroup);

    // -- cursor shape --
    auto cursorGroup = new QGroupBox("Cursor");
    auto cgl = Layout::horizontal(cursorGroup, false);
    auto cursorCombo = new QComboBox();
    cursorCombo->addItems({"pointer", "arrow", "hand", "ibeam", "wait", "cross", "forbidden"});
    cgl->addWidget(cursorCombo);
    QObject::connect(cursorCombo, &QComboBox::currentTextChanged, &app,
        [boxes](const QString& name)
        {
            for (auto* box : boxes)
            {
                box->setCursorShapeName(name);
            }
        });
    cl->addWidget(cursorGroup);

    // -- ripple opacity --
    auto opGroup = new QGroupBox("Ripple opacity");
    auto ol = Layout::horizontal(opGroup, false);
    auto opSpin = new QDoubleSpinBox();
    opSpin->setRange(0.0, 1.0);
    opSpin->setDecimals(3);
    opSpin->setSingleStep(0.01);
    opSpin->setValue(0.10);
    ol->addWidget(opSpin);
    QObject::connect(opSpin, &QDoubleSpinBox::valueChanged, &app,
        [boxes](double opacity)
        {
            for (auto* box : boxes)
            {
                if (auto* ripple=box->rippleOverlay())
                {
                    ripple->setRippleOpacity(opacity);
                }
            }
        });
    cl->addWidget(opGroup);

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
    w.resize(900, 500);
    w.setWindowTitle("CheckBox / RadioBox Demo");
    w.show();
    return app.exec();
}

//--------------------------------------------------------------------------
