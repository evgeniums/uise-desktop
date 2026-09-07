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

/** @file demo/countbadge/main.cpp
*
*  Demo application of CountBadge – circle-to-pill counter badge.
*
*/

/****************************************************************************/

#include <vector>

#include <QApplication>
#include <QMainWindow>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QCheckBox>
#include <QSpinBox>
#include <QGroupBox>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/style.hpp>
#include <uise/desktop/countbadge.hpp>

using namespace UISE_DESKTOP_NAMESPACE;

//--------------------------------------------------------------------------

//! The value range the badge is actually fed in production: plain counts up to 3 digits, then
//! the abbreviated k/M forms. Used to build both comparison rows below.
static const std::vector<QString> SampleValues = {"1", "9", "42", "999", "1.2K", "99K", "12M"};

//! A caption + widget stacked vertically, used for every sample in both rows so the two rows
//! line up value-for-value.
QWidget* labeledSample(const QString& caption, QWidget* sample)
{
    auto frame = new QFrame();
    auto l = Layout::vertical(frame, false);
    l->setAlignment(Qt::AlignCenter);
    auto captionLabel = new QLabel(caption);
    captionLabel->setAlignment(Qt::AlignCenter);
    l->addWidget(sample, 0, Qt::AlignCenter);
    l->addWidget(captionLabel, 0, Qt::AlignCenter);
    return frame;
}

//! Reproduces the OLD unread-count treatment (a QLabel styled with QSS border-radius) exactly
//! as used in chatlist.qss/subjectitem.qss today -- so the instability it produces (height and
//! roundness both change with the text) is visible right next to CountBadge's fixed silhouette.
QLabel* oldStyleLabel(const QString& text)
{
    auto label = new QLabel(text);
    label->setAlignment(Qt::AlignCenter);
    label->setStyleSheet(
        "QLabel {"
        "  background-color: #4895ef;"
        "  color: #FFFFFF;"
        "  font-weight: bold;"
        "  padding: 0px 2px;"
        "  border-radius: 8px;"
        "  max-height: 20px;"
        "}");
    return label;
}

//--------------------------------------------------------------------------

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // Load the bundled QSS (includes our light/dark countbadge.qss).
    Style::instance().applyStyleSheet();

    // Extra base QSS that styles the demo chrome for each theme.
    const QString darkChrome  = "QFrame#mainFrame { background-color: #1a1a1a; }"
                                "QLabel { color: #cccccc; }"
                                "QGroupBox { color: #cccccc; border: 1px solid #555; margin-top: 6px; }"
                                "QGroupBox::title { subcontrol-origin: margin; left: 8px; }";
    const QString lightChrome = "QFrame#mainFrame { background-color: #f0f0f0; }"
                                "QLabel { color: #222222; }"
                                "QGroupBox { color: #222222; border: 1px solid #aaa; margin-top: 6px; }"
                                "QGroupBox::title { subcontrol-origin: margin; left: 8px; }";

    QMainWindow w;
    auto mainFrame = new QFrame();
    mainFrame->setObjectName("mainFrame");
    auto ml = Layout::vertical(mainFrame, false);

    // ---- preview area: new CountBadge row, then the old QLabel row for comparison ----
    auto previewFrame = new QFrame();
    previewFrame->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    ml->addWidget(previewFrame, 1);
    auto pfl = Layout::vertical(previewFrame, false);
    pfl->setAlignment(Qt::AlignCenter);

    auto newRowTitle = new QLabel("CountBadge (constant height, circle -> pill)");
    newRowTitle->setAlignment(Qt::AlignCenter);
    pfl->addWidget(newRowTitle, 0, Qt::AlignCenter);

    auto newRow = new QFrame();
    auto nrl = Layout::horizontal(newRow, false);
    nrl->setAlignment(Qt::AlignCenter);
    nrl->setSpacing(24);
    std::vector<CountBadge*> sampleBadges;
    for (const auto& value : SampleValues)
    {
        auto badge = new CountBadge(newRow);
        badge->setText(value);
        sampleBadges.push_back(badge);
        nrl->addWidget(labeledSample(value, badge), 0, Qt::AlignCenter);
    }
    pfl->addWidget(newRow, 0, Qt::AlignCenter);

    auto oldRowTitle = new QLabel("Old QLabel + border-radius (height/roundness wobble)");
    oldRowTitle->setAlignment(Qt::AlignCenter);
    pfl->addWidget(oldRowTitle, 0, Qt::AlignCenter);

    auto oldRow = new QFrame();
    auto orl = Layout::horizontal(oldRow, false);
    orl->setAlignment(Qt::AlignCenter);
    orl->setSpacing(24);
    for (const auto& value : SampleValues)
    {
        orl->addWidget(labeledSample(value, oldStyleLabel(value)), 0, Qt::AlignCenter);
    }
    pfl->addWidget(oldRow, 0, Qt::AlignCenter);

    // A single live badge driven by the controls below.
    auto liveTitle = new QLabel("Live");
    liveTitle->setAlignment(Qt::AlignCenter);
    pfl->addWidget(liveTitle, 0, Qt::AlignCenter);
    auto liveBadge = new CountBadge(previewFrame);
    liveBadge->setText("7");
    auto liveRow = new QFrame();
    auto lrl = Layout::horizontal(liveRow, false);
    lrl->setAlignment(Qt::AlignCenter);
    lrl->addWidget(liveBadge, 0, Qt::AlignCenter);
    pfl->addWidget(liveRow, 0, Qt::AlignCenter);

    // ---- controls area ----
    auto controlsFrame = new QFrame();
    ml->addWidget(controlsFrame, 0);
    auto cl = Layout::horizontal(controlsFrame, false);
    cl->setAlignment(Qt::AlignLeft);

    // -- text input --
    auto textGroup = new QGroupBox("Text");
    auto tl = Layout::horizontal(textGroup, false);
    auto textEdit = new QLineEdit("7");
    textEdit->setMinimumWidth(100);
    tl->addWidget(textEdit);
    QObject::connect(textEdit, &QLineEdit::textChanged, liveBadge, &CountBadge::setText);
    cl->addWidget(textGroup);

    // -- count spinbox (drives setCount() directly, overwriting the text field) --
    auto countGroup = new QGroupBox("Count");
    auto col = Layout::horizontal(countGroup, false);
    auto countSpin = new QSpinBox();
    countSpin->setRange(0, 999999999);
    countSpin->setValue(7);
    col->addWidget(countSpin);
    QObject::connect(countSpin, &QSpinBox::valueChanged, liveBadge,
        [liveBadge, textEdit](int value)
        {
            QSignalBlocker blocker(textEdit);
            liveBadge->setCount(static_cast<size_t>(value));
            textEdit->setText(QString::number(value));
        });
    cl->addWidget(countGroup);

    // -- mute toggle --
    auto muteGroup = new QGroupBox("Style");
    auto mgl = Layout::horizontal(muteGroup, false);
    auto muteCheck = new QCheckBox("Muted");
    mgl->addWidget(muteCheck);
    QObject::connect(muteCheck, &QCheckBox::toggled, liveBadge, &CountBadge::setMuted);
    cl->addWidget(muteGroup);

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
            Style::repolishRecursive(mainFrame);
        });
    thl->addWidget(themeButton);
    cl->addWidget(themeGroup);

    // Apply initial chrome QSS.
    Style::instance().setBaseQss(startDark ? darkChrome : lightChrome);
    Style::instance().applyStyleSheet(true);

    w.setCentralWidget(mainFrame);
    w.resize(900, 420);
    w.setWindowTitle("CountBadge Demo");
    w.show();
    return app.exec();
}

//--------------------------------------------------------------------------
