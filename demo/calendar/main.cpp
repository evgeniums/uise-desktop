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

/** @file demo/calendar/main.cpp
*
*  Demo application of Calendar and CalendarInput -- the month-grid calendar widget.
*
*/

/****************************************************************************/

#include <vector>
#include <utility>

#include <QApplication>
#include <QMainWindow>
#include <QScrollArea>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QGroupBox>
#include <QTextEdit>
#include <QDateEdit>
#include <QLocale>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/style.hpp>
#include <uise/desktop/calendar.hpp>
#include <uise/desktop/calendarinput.hpp>

using namespace UISE_DESKTOP_NAMESPACE;

//--------------------------------------------------------------------------

namespace {

QString modeName(CalendarMode mode)
{
    switch (mode)
    {
        case (CalendarMode::Activation): return QStringLiteral("Activation");
        case (CalendarMode::SingleSelection): return QStringLiteral("SingleSelection");
        case (CalendarMode::RangeSelection): return QStringLiteral("RangeSelection");
        case (CalendarMode::MultipleSelection): return QStringLiteral("MultipleSelection");
        case (CalendarMode::Auto): return QStringLiteral("Auto");
        case (CalendarMode::ExtendedSelection): return QStringLiteral("ExtendedSelection");
    }
    return QString();
}

}

//--------------------------------------------------------------------------

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // Load the bundled QSS (includes our light/dark calendar.qss).
    Style::instance().applyStyleSheet();

    // Extra base QSS that styles the demo chrome for each theme.
    const QString darkChrome  = "QFrame#mainFrame { background-color: #1a1a1a; }"
                                "QGroupBox { color: #cccccc; border: 1px solid #555; margin-top: 6px; }"
                                "QGroupBox::title { subcontrol-origin: margin; left: 8px; }"
                                "QTextEdit { background-color: #111111; color: #dddddd; }";
    const QString lightChrome = "QFrame#mainFrame { background-color: #f0f0f0; }"
                                "QGroupBox { color: #222222; border: 1px solid #aaa; margin-top: 6px; }"
                                "QGroupBox::title { subcontrol-origin: margin; left: 8px; }"
                                "QTextEdit { background-color: #ffffff; color: #111111; }";

    QMainWindow w;
    auto scrollArea=new QScrollArea();
    scrollArea->setWidgetResizable(true);
    auto mainFrame = new QFrame();
    mainFrame->setObjectName("mainFrame");
    scrollArea->setWidget(mainFrame);
    auto ml = Layout::vertical(mainFrame, false);

    // ---- preview ----
    auto previewFrame = new QFrame();
    ml->addWidget(previewFrame, 0);
    auto pfl = Layout::horizontal(previewFrame, false);
    pfl->setAlignment(Qt::AlignLeft);

    auto calendarGroup = new QGroupBox("Calendar (Auto mode)");
    auto cgl = Layout::vertical(calendarGroup, false);
    auto calendar = new Calendar(CalendarMode::Auto, calendarGroup);
    cgl->addWidget(calendar);
    pfl->addWidget(calendarGroup);

    auto inputsGroup = new QGroupBox("CalendarInput per mode");
    auto igl = Layout::vertical(inputsGroup, false);

    auto activationInput = new CalendarInput(CalendarMode::Activation);
    activationInput->setObjectName("ActivationInput");
    auto singleInput = new CalendarInput(CalendarMode::SingleSelection);
    singleInput->setObjectName("SingleInput");
    auto rangeInput = new CalendarInput(CalendarMode::RangeSelection);
    rangeInput->setObjectName("RangeInput");
    auto multipleInput = new CalendarInput(CalendarMode::MultipleSelection);
    multipleInput->setObjectName("MultipleInput");
    auto autoInput = new CalendarInput(CalendarMode::Auto);
    autoInput->setObjectName("AutoInput");
    auto extendedInput = new CalendarInput(CalendarMode::ExtendedSelection);
    extendedInput->setObjectName("ExtendedInput");

    auto addInputRow = [igl](const QString& label, QWidget* input)
    {
        auto row = new QFrame();
        auto rl = Layout::horizontal(row, false);
        auto lb = new QLabel(label);
        lb->setMinimumWidth(140);
        rl->addWidget(lb);
        rl->addWidget(input, 1);
        igl->addWidget(row);
    };
    addInputRow("Activation", activationInput);
    addInputRow("SingleSelection", singleInput);
    addInputRow("RangeSelection", rangeInput);
    addInputRow("MultipleSelection", multipleInput);
    addInputRow("Auto", autoInput);
    addInputRow("ExtendedSelection", extendedInput);
    pfl->addWidget(inputsGroup, 1);

    std::vector<Calendar*> allCalendars{
        calendar,
        activationInput->calendar(),
        singleInput->calendar(),
        rangeInput->calendar(),
        multipleInput->calendar(),
        autoInput->calendar(),
        extendedInput->calendar()
    };
    std::vector<CalendarInput*> allInputs{
        activationInput, singleInput, rangeInput, multipleInput, autoInput, extendedInput
    };

    // ---- log ----
    auto log = new QTextEdit();
    log->setReadOnly(true);
    log->setMaximumHeight(160);
    ml->addWidget(log, 1);

    auto logMsg = [log](const QString& source, const QString& value)
    {
        log->append(QString("%1: %2").arg(source, value));
        log->ensureCursorVisible();
    };

    QObject::connect(calendar, &Calendar::dateActivated, [logMsg](const QDate& d)
    {
        logMsg("Calendar.dateActivated", d.toString(Qt::ISODate));
    });
    QObject::connect(calendar, &Calendar::selectionChanged, [logMsg, calendar]()
    {
        logMsg("Calendar.selectionChanged", calendar->hasSelection() ? calendar->title() : QStringLiteral("(empty)"));
    });
    QObject::connect(calendar, &Calendar::rangeSelected, [logMsg](const QDate& from, const QDate& to)
    {
        logMsg("Calendar.rangeSelected", QString("%1 - %2").arg(from.toString(Qt::ISODate), to.toString(Qt::ISODate)));
    });
    QObject::connect(calendar, &Calendar::selectedDatesChanged, [logMsg](const QList<QDate>& dates)
    {
        QStringList parts;
        for (auto&& d: dates)
        {
            parts.append(d.toString(Qt::ISODate));
        }
        logMsg("Calendar.selectedDatesChanged", parts.join(", "));
    });
    QObject::connect(calendar, &Calendar::selectionCleared, [logMsg]()
    {
        logMsg("Calendar.selectionCleared", QString());
    });
    QObject::connect(calendar, &Calendar::displayedMonthChanged, [logMsg](const QDate& month)
    {
        logMsg("Calendar.displayedMonthChanged", month.toString("yyyy-MM"));
    });
    QObject::connect(calendar, &Calendar::effectiveModeChanged, [logMsg](CalendarMode mode)
    {
        logMsg("Calendar.effectiveModeChanged", modeName(mode));
    });

    for (auto&& input: allInputs)
    {
        QObject::connect(input, &CalendarInput::dateActivated, [logMsg, input](const QDate& d)
        {
            logMsg(QString("%1.dateActivated").arg(input->objectName()), d.toString(Qt::ISODate));
        });
        QObject::connect(input, &CalendarInput::selectionChanged, [logMsg, input]()
        {
            logMsg(QString("%1.selectionChanged").arg(input->objectName()), input->text());
        });
    }

    // ---- controls ----
    auto controlsFrame = new QFrame();
    ml->addWidget(controlsFrame, 0);
    auto cl = Layout::horizontal(controlsFrame, false);
    cl->setAlignment(Qt::AlignLeft);

    // -- mode --
    auto modeGroup = new QGroupBox("Calendar mode");
    auto mgl = Layout::vertical(modeGroup, false);
    auto modeCombo = new QComboBox();
    modeCombo->addItem("Activation", static_cast<int>(CalendarMode::Activation));
    modeCombo->addItem("SingleSelection", static_cast<int>(CalendarMode::SingleSelection));
    modeCombo->addItem("RangeSelection", static_cast<int>(CalendarMode::RangeSelection));
    modeCombo->addItem("MultipleSelection", static_cast<int>(CalendarMode::MultipleSelection));
    modeCombo->addItem("Auto", static_cast<int>(CalendarMode::Auto));
    modeCombo->addItem("ExtendedSelection", static_cast<int>(CalendarMode::ExtendedSelection));
    modeCombo->setCurrentIndex(4);
    mgl->addWidget(modeCombo);
    QObject::connect(modeCombo, &QComboBox::currentIndexChanged, [calendar, modeCombo](int idx)
    {
        calendar->setMode(static_cast<CalendarMode>(modeCombo->itemData(idx).toInt()));
    });
    cl->addWidget(modeGroup);

    // -- week start --
    auto weekStartGroup = new QGroupBox("Week start");
    auto wsgl = Layout::vertical(weekStartGroup, false);
    auto weekStartCombo = new QComboBox();
    weekStartCombo->addItems({"Locale", "Monday", "Sunday"});
    wsgl->addWidget(weekStartCombo);
    QObject::connect(weekStartCombo, &QComboBox::currentIndexChanged, [allCalendars](int idx)
    {
        auto value = static_cast<CalendarWeekStart>(idx);
        for (auto* c : allCalendars)
        {
            c->setWeekStart(value);
        }
    });
    cl->addWidget(weekStartGroup);

    // -- date range --
    auto rangeGroup = new QGroupBox("Date range (applies to all)");
    auto rgl = Layout::vertical(rangeGroup, false);
    auto minEdit = new QDateEdit(QDate(2000, 1, 1));
    minEdit->setCalendarPopup(true);
    auto noMinCheck = new QCheckBox("No minimum");
    noMinCheck->setChecked(true);
    auto maxEdit = new QDateEdit(QDate::currentDate());
    maxEdit->setCalendarPopup(true);
    auto noMaxCheck = new QCheckBox("No maximum (default: today)");
    rgl->addWidget(new QLabel("Min"));
    rgl->addWidget(minEdit);
    rgl->addWidget(noMinCheck);
    rgl->addWidget(new QLabel("Max"));
    rgl->addWidget(maxEdit);
    rgl->addWidget(noMaxCheck);
    auto applyRangeBtn = new QPushButton("Apply range");
    rgl->addWidget(applyRangeBtn);
    QObject::connect(applyRangeBtn, &QPushButton::clicked, [allCalendars, minEdit, maxEdit, noMinCheck, noMaxCheck]()
    {
        auto min = noMinCheck->isChecked() ? QDate() : minEdit->date();
        auto max = noMaxCheck->isChecked() ? QDate() : maxEdit->date();
        for (auto* c : allCalendars)
        {
            c->setDateRange(min, max);
        }
    });
    cl->addWidget(rangeGroup);

    // -- current date --
    auto currentDateGroup = new QGroupBox("Current date (\"today\")");
    auto cdgl = Layout::vertical(currentDateGroup, false);
    auto currentDateEdit = new QDateEdit(QDate::currentDate());
    currentDateEdit->setCalendarPopup(true);
    cdgl->addWidget(currentDateEdit);
    auto applyCurrentDateBtn = new QPushButton("Apply");
    cdgl->addWidget(applyCurrentDateBtn);
    QObject::connect(applyCurrentDateBtn, &QPushButton::clicked, [allCalendars, currentDateEdit]()
    {
        for (auto* c : allCalendars)
        {
            c->setCurrentDate(currentDateEdit->date());
        }
    });
    cl->addWidget(currentDateGroup);

    // -- navigation --
    auto navGroup = new QGroupBox("Navigation");
    auto ngl = Layout::vertical(navGroup, false);
    auto prevBtn = new QPushButton("Previous month");
    auto nextBtn = new QPushButton("Next month");
    auto todayBtn = new QPushButton("Today");
    auto clearBtn = new QPushButton("Clear selection");
    ngl->addWidget(prevBtn);
    ngl->addWidget(nextBtn);
    ngl->addWidget(todayBtn);
    ngl->addWidget(clearBtn);
    QObject::connect(prevBtn, &QPushButton::clicked, calendar, &Calendar::showPreviousMonth);
    QObject::connect(nextBtn, &QPushButton::clicked, calendar, &Calendar::showNextMonth);
    QObject::connect(todayBtn, &QPushButton::clicked, calendar, &Calendar::showToday);
    QObject::connect(clearBtn, &QPushButton::clicked, calendar, &Calendar::clearSelection);
    cl->addWidget(navGroup);

    // -- cell size --
    auto cellSizeGroup = new QGroupBox("Cell size");
    auto csgl = Layout::horizontal(cellSizeGroup, false);
    auto cellSizeSpin = new QSpinBox();
    cellSizeSpin->setRange(20, 60);
    cellSizeSpin->setValue(34);
    csgl->addWidget(cellSizeSpin);
    QObject::connect(cellSizeSpin, &QSpinBox::valueChanged, [allCalendars](int val)
    {
        for (auto* c : allCalendars)
        {
            c->setCellWidth(val);
            c->setCellHeight(val);
        }
    });
    cl->addWidget(cellSizeGroup);

    // -- header --
    auto headerGroup = new QGroupBox("Header");
    auto hgl = Layout::vertical(headerGroup, false);
    auto headerClickableCheck = new QCheckBox("Header clickable");
    headerClickableCheck->setChecked(true);
    hgl->addWidget(headerClickableCheck);
    QObject::connect(headerClickableCheck, &QCheckBox::toggled, [allCalendars](bool enable)
    {
        for (auto* c : allCalendars)
        {
            c->setHeaderClickable(enable);
        }
    });
    auto openMonthPickerBtn = new QPushButton("Open month picker");
    hgl->addWidget(openMonthPickerBtn);
    QObject::connect(openMonthPickerBtn, &QPushButton::clicked, calendar, &Calendar::openMonthPicker);
    cl->addWidget(headerGroup);

    // -- title cap --
    auto titleCapGroup = new QGroupBox("Max title dates (0=unlimited)");
    auto tcgl = Layout::horizontal(titleCapGroup, false);
    auto titleCapSpin = new QSpinBox();
    titleCapSpin->setRange(0, 10);
    tcgl->addWidget(titleCapSpin);
    QObject::connect(titleCapSpin, &QSpinBox::valueChanged, [allCalendars](int val)
    {
        for (auto* c : allCalendars)
        {
            c->setMaxTitleDates(val);
        }
    });
    cl->addWidget(titleCapGroup);

    // -- dropdown buttons policy --
    auto buttonsGroup = new QGroupBox("Input popup");
    auto bgl = Layout::vertical(buttonsGroup, false);
    auto explicitUpdateCheck = new QCheckBox("Explicit update (Apply/Cancel)");
    explicitUpdateCheck->setChecked(false);
    auto autoCloseCheck = new QCheckBox("Auto close");
    autoCloseCheck->setChecked(true);
    bgl->addWidget(explicitUpdateCheck);
    bgl->addWidget(autoCloseCheck);
    QObject::connect(explicitUpdateCheck, &QCheckBox::toggled, [allInputs](bool enable)
    {
        for (auto* i : allInputs)
        {
            i->setUpdateMode(enable ? CalendarUpdateMode::Explicit : CalendarUpdateMode::Auto);
        }
    });
    QObject::connect(autoCloseCheck, &QCheckBox::toggled, [allInputs](bool enable)
    {
        for (auto* i : allInputs)
        {
            i->dropdown()->setAutoClose(enable);
        }
    });
    cl->addWidget(buttonsGroup);

    // -- locale: exercises week start order, weekend columns, month names and short date form --
    auto localeGroup = new QGroupBox("Locale");
    auto lgl = Layout::horizontal(localeGroup, false);
    auto localeCombo = new QComboBox();
    localeCombo->addItem("en_US", QVariant::fromValue(QLocale(QLocale::English, QLocale::UnitedStates)));
    localeCombo->addItem("de_DE", QVariant::fromValue(QLocale(QLocale::German, QLocale::Germany)));
    localeCombo->addItem("ru_RU", QVariant::fromValue(QLocale(QLocale::Russian, QLocale::Russia)));
    localeCombo->addItem("ja_JP", QVariant::fromValue(QLocale(QLocale::Japanese, QLocale::Japan)));
    localeCombo->addItem("hu_HU", QVariant::fromValue(QLocale(QLocale::Hungarian, QLocale::Hungary)));
    lgl->addWidget(localeCombo);
    QObject::connect(localeCombo, &QComboBox::currentIndexChanged, [allCalendars, allInputs, localeCombo](int idx)
    {
        auto locale = localeCombo->itemData(idx).value<QLocale>();
        for (auto* c : allCalendars)
        {
            c->setLocale(locale);
        }
        for (auto* i : allInputs)
        {
            i->setLocale(locale);
        }
    });
    cl->addWidget(localeGroup);

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

    w.setCentralWidget(scrollArea);
    w.resize(1250, 820);
    w.setWindowTitle("Calendar Demo");
    w.show();
    return app.exec();
}

//--------------------------------------------------------------------------
