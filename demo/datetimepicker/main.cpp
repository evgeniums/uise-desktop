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

/** @file demo/datetimepicker/main.cpp
*
*  Demo application of DateTimePicker / DatePicker / MonthPicker / TimePicker and the
*  DateInput / MonthInput / TimeInput / DateTimeInput dropdown fields for date/time picking.
*
*/

/****************************************************************************/

#include <vector>
#include <utility>

#include <QApplication>
#include <QMainWindow>
#include <QLabel>
#include <QLineEdit>
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
#include <uise/desktop/datetimepicker.hpp>
#include <uise/desktop/datetimeinput.hpp>

using namespace UISE_DESKTOP_NAMESPACE;

//--------------------------------------------------------------------------

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // Load the bundled QSS (includes our light/dark datetimepicker.qss).
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
    auto mainFrame = new QFrame();
    mainFrame->setObjectName("mainFrame");
    auto ml = Layout::vertical(mainFrame, false);

    // ---- preview: pickers ----
    auto pickersFrame = new QFrame();
    ml->addWidget(pickersFrame, 0);
    auto pfl = Layout::horizontal(pickersFrame, false);
    pfl->setAlignment(Qt::AlignLeft);

    auto datePicker = new DatePicker(pickersFrame);
    auto monthPicker = new MonthPicker(pickersFrame);
    auto timePicker = new TimePicker(pickersFrame);
    auto dateTimePicker = new DateTimePicker(pickersFrame);

    auto addPickerGroup = [pfl](const QString& title, QWidget* picker)
    {
        auto group = new QGroupBox(title);
        auto l = Layout::vertical(group, false);
        l->addWidget(picker);
        pfl->addWidget(group);
    };
    addPickerGroup("DatePicker", datePicker);
    addPickerGroup("MonthPicker", monthPicker);
    addPickerGroup("TimePicker", timePicker);
    addPickerGroup("DateTimePicker (all fields)", dateTimePicker);
    pfl->addStretch(1);

    // ---- preview: inputs ----
    auto inputsFrame = new QFrame();
    ml->addWidget(inputsFrame, 0);
    auto ifl = Layout::vertical(inputsFrame, false);

    auto dateInput = new DateInput();
    auto monthInput = new MonthInput();
    auto timeInput = new TimeInput();
    auto dateTimeInput = new DateTimeInput();

    auto addInputRow = [ifl](const QString& label, QWidget* input)
    {
        auto row = new QFrame();
        auto rl = Layout::horizontal(row, false);
        auto lb = new QLabel(label);
        lb->setMinimumWidth(110);
        rl->addWidget(lb);
        rl->addWidget(input, 1);
        ifl->addWidget(row);
    };
    addInputRow("DateInput", dateInput);
    addInputRow("MonthInput", monthInput);
    addInputRow("TimeInput", timeInput);
    addInputRow("DateTimeInput", dateTimeInput);

    // ---- log ----
    auto log = new QTextEdit();
    log->setReadOnly(true);
    log->setMinimumHeight(120);
    log->setMaximumHeight(180);
    ml->addWidget(log, 1);

    auto logMsg = [log](const QString& source, const QString& value)
    {
        log->append(QString("%1: %2").arg(source, value));
        log->ensureCursorVisible();
    };

    QObject::connect(datePicker, &DateTimePicker::dateChanged, [logMsg](const QDate& v)
    {
        logMsg("DatePicker.dateChanged", v.toString(Qt::ISODate));
    });
    QObject::connect(monthPicker, &DateTimePicker::dateChanged, [logMsg](const QDate& v)
    {
        logMsg("MonthPicker.dateChanged", v.toString(Qt::ISODate));
    });
    QObject::connect(timePicker, &DateTimePicker::timeChanged, [logMsg](const QTime& v)
    {
        logMsg("TimePicker.timeChanged", v.toString(Qt::ISODate));
    });
    QObject::connect(dateTimePicker, &DateTimePicker::dateTimeChanged, [logMsg](const QDateTime& v)
    {
        logMsg("DateTimePicker.dateTimeChanged", v.toString(Qt::ISODate));
    });

    QObject::connect(dateInput, &DateInput::dateChanged, [logMsg](const QDate& v)
    {
        logMsg("DateInput.dateChanged", v.toString(Qt::ISODate));
    });
    QObject::connect(monthInput, &MonthInput::dateChanged, [logMsg](const QDate& v)
    {
        logMsg("MonthInput.dateChanged", v.toString(Qt::ISODate));
    });
    QObject::connect(timeInput, &TimeInput::timeChanged, [logMsg](const QTime& v)
    {
        logMsg("TimeInput.timeChanged", v.toString(Qt::ISODate));
    });
    QObject::connect(dateTimeInput, &DateTimeInput::dateTimeChanged, [logMsg](const QDateTime& v)
    {
        logMsg("DateTimeInput.dateTimeChanged", v.toString(Qt::ISODate));
    });

    std::vector<DateTimePicker*> allPickers{datePicker, monthPicker, timePicker, dateTimePicker};
    std::vector<DateTimeInput*> allInputs{dateInput, monthInput, timeInput, dateTimeInput};

    // ---- controls ----
    auto controlsFrame = new QFrame();
    ml->addWidget(controlsFrame, 0);
    auto cl = Layout::horizontal(controlsFrame, false);
    cl->setAlignment(Qt::AlignLeft);

    // -- fields of the plain DateTimePicker: toggling Day off turns it into month mode live --
    struct FieldCheck { DateTimeField field; QCheckBox* box; };
    auto fieldsGroup = new QGroupBox("DateTimePicker fields");
    auto fgl = Layout::vertical(fieldsGroup, false);
    auto fieldChecks = std::make_shared<std::vector<FieldCheck>>();
    for (auto&& f: std::vector<std::pair<QString,DateTimeField>>{
            {"Year", DateTimeField::Year}, {"Month", DateTimeField::Month}, {"Day", DateTimeField::Day},
            {"Hour", DateTimeField::Hour}, {"Minute", DateTimeField::Minute}, {"Second", DateTimeField::Second}})
    {
        auto box = new QCheckBox(f.first);
        box->setChecked(dateTimePicker->fields().testFlag(f.second));
        fgl->addWidget(box);
        fieldChecks->push_back({f.second, box});
    }
    auto updateFields = [dateTimePicker, fieldChecks]()
    {
        DateTimeFields fields;
        for (auto&& fc: *fieldChecks)
        {
            if (fc.box->isChecked())
            {
                fields |= fc.field;
            }
        }
        dateTimePicker->setFields(fields);
    };
    for (auto&& fc: *fieldChecks)
    {
        QObject::connect(fc.box, &QCheckBox::toggled, updateFields);
    }
    cl->addWidget(fieldsGroup);

    // -- date range --
    auto rangeGroup = new QGroupBox("Date range (applies to all)");
    auto rgl = Layout::vertical(rangeGroup, false);
    auto minEdit = new QDateEdit(QDate(1900, 1, 1));
    minEdit->setCalendarPopup(true);
    auto maxEdit = new QDateEdit(QDate(2100, 12, 31));
    maxEdit->setCalendarPopup(true);
    rgl->addWidget(new QLabel("Min"));
    rgl->addWidget(minEdit);
    rgl->addWidget(new QLabel("Max"));
    rgl->addWidget(maxEdit);
    auto applyRangeBtn = new QPushButton("Apply range");
    rgl->addWidget(applyRangeBtn);
    QObject::connect(applyRangeBtn, &QPushButton::clicked, [allPickers, allInputs, minEdit, maxEdit]()
    {
        for (auto* p: allPickers)
        {
            p->setDateRange(minEdit->date(), maxEdit->date());
        }
        for (auto* i: allInputs)
        {
            i->setDateRange(minEdit->date(), maxEdit->date());
        }
    });
    cl->addWidget(rangeGroup);

    // -- minute step --
    auto minuteStepGroup = new QGroupBox("Minute step");
    auto msl = Layout::horizontal(minuteStepGroup, false);
    auto minuteStepCombo = new QComboBox();
    minuteStepCombo->addItems({"1", "5", "10", "15", "30"});
    msl->addWidget(minuteStepCombo);
    QObject::connect(minuteStepCombo, &QComboBox::currentTextChanged, [allPickers, allInputs](const QString& text)
    {
        auto step = text.toInt();
        for (auto* p: allPickers)
        {
            p->setMinuteStep(step);
        }
        for (auto* i: allInputs)
        {
            i->picker()->setMinuteStep(step);
        }
    });
    cl->addWidget(minuteStepGroup);

    // -- month format --
    auto monthFormatGroup = new QGroupBox("Month format");
    auto mfl = Layout::horizontal(monthFormatGroup, false);
    auto monthFormatCombo = new QComboBox();
    monthFormatCombo->addItems({"Long name", "Short name", "Number"});
    mfl->addWidget(monthFormatCombo);
    QObject::connect(monthFormatCombo, &QComboBox::currentIndexChanged, [allPickers, allInputs](int idx)
    {
        auto fmt = static_cast<DateTimePicker::MonthFormat>(idx);
        for (auto* p: allPickers)
        {
            p->setMonthFormat(fmt);
        }
        for (auto* i: allInputs)
        {
            i->picker()->setMonthFormat(fmt);
        }
    });
    cl->addWidget(monthFormatGroup);

    // -- visible rows --
    auto rowsGroup = new QGroupBox("Visible rows");
    auto rl = Layout::horizontal(rowsGroup, false);
    auto rowsCombo = new QComboBox();
    rowsCombo->addItems({"3", "5", "7"});
    rowsCombo->setCurrentIndex(1);
    rl->addWidget(rowsCombo);
    QObject::connect(rowsCombo, &QComboBox::currentTextChanged, [allPickers, allInputs](const QString& text)
    {
        auto rows = text.toInt();
        for (auto* p: allPickers)
        {
            p->setVisibleRows(rows);
        }
        for (auto* i: allInputs)
        {
            i->picker()->setVisibleRows(rows);
        }
    });
    cl->addWidget(rowsGroup);

    // -- item height --
    auto heightGroup = new QGroupBox("Item height (0=auto)");
    auto hgl = Layout::horizontal(heightGroup, false);
    auto heightSpin = new QSpinBox();
    heightSpin->setRange(0, 80);
    heightSpin->setValue(0);
    hgl->addWidget(heightSpin);
    QObject::connect(heightSpin, &QSpinBox::valueChanged, [allPickers, allInputs](int val)
    {
        for (auto* p: allPickers)
        {
            p->setItemHeight(val);
        }
        for (auto* i: allInputs)
        {
            i->picker()->setItemHeight(val);
        }
    });
    cl->addWidget(heightGroup);

    // -- circular fields --
    struct CircularCheck { DateTimeField field; QCheckBox* box; };
    auto circularGroup = new QGroupBox("Circular wheels");
    auto cgl = Layout::vertical(circularGroup, false);
    auto circularChecks = std::make_shared<std::vector<CircularCheck>>();
    for (auto&& f: std::vector<std::pair<QString,DateTimeField>>{
            {"Month", DateTimeField::Month}, {"Day", DateTimeField::Day},
            {"Hour", DateTimeField::Hour}, {"Minute", DateTimeField::Minute}, {"Second", DateTimeField::Second}})
    {
        auto box = new QCheckBox(f.first);
        box->setChecked(true);
        cgl->addWidget(box);
        circularChecks->push_back({f.second, box});
    }
    auto updateCircular = [allPickers, allInputs, circularChecks]()
    {
        DateTimeFields fields;
        for (auto&& cc: *circularChecks)
        {
            if (cc.box->isChecked())
            {
                fields |= cc.field;
            }
        }
        for (auto* p: allPickers)
        {
            p->setCircularFields(fields);
        }
        for (auto* i: allInputs)
        {
            i->picker()->setCircularFields(fields);
        }
    };
    for (auto&& cc: *circularChecks)
    {
        QObject::connect(cc.box, &QCheckBox::toggled, updateCircular);
    }
    cl->addWidget(circularGroup);

    // -- separators --
    auto separatorsGroup = new QGroupBox("Separators");
    auto sgl = Layout::horizontal(separatorsGroup, false);
    auto separatorsCheck = new QCheckBox("Show column separators");
    sgl->addWidget(separatorsCheck);
    QObject::connect(separatorsCheck, &QCheckBox::toggled, [allPickers, allInputs](bool enable)
    {
        for (auto* p: allPickers)
        {
            p->setSeparatorsVisible(enable);
        }
        for (auto* i: allInputs)
        {
            i->picker()->setSeparatorsVisible(enable);
        }
    });
    cl->addWidget(separatorsGroup);

    // -- popup buttons --
    auto buttonsGroup = new QGroupBox("Input popup");
    auto bgl = Layout::horizontal(buttonsGroup, false);
    auto buttonsCheck = new QCheckBox("Show Apply/Cancel");
    bgl->addWidget(buttonsCheck);
    QObject::connect(buttonsCheck, &QCheckBox::toggled, [allInputs](bool enable)
    {
        for (auto* i: allInputs)
        {
            i->setButtonsVisible(enable);
        }
    });
    cl->addWidget(buttonsGroup);

    // -- locale: the highest-value control -- exercises column reorder, standalone month
    //    names and 12/24-hour switching in one click --
    auto localeGroup = new QGroupBox("Locale");
    auto lgl = Layout::horizontal(localeGroup, false);
    auto localeCombo = new QComboBox();
    localeCombo->addItem("en_US", QVariant::fromValue(QLocale(QLocale::English, QLocale::UnitedStates)));
    localeCombo->addItem("de_DE", QVariant::fromValue(QLocale(QLocale::German, QLocale::Germany)));
    localeCombo->addItem("ru_RU", QVariant::fromValue(QLocale(QLocale::Russian, QLocale::Russia)));
    localeCombo->addItem("ja_JP", QVariant::fromValue(QLocale(QLocale::Japanese, QLocale::Japan)));
    localeCombo->addItem("hu_HU", QVariant::fromValue(QLocale(QLocale::Hungarian, QLocale::Hungary)));
    lgl->addWidget(localeCombo);
    QObject::connect(localeCombo, &QComboBox::currentIndexChanged, [allPickers, allInputs, localeCombo](int idx)
    {
        auto locale = localeCombo->itemData(idx).value<QLocale>();
        for (auto* p: allPickers)
        {
            p->setLocale(locale);
        }
        for (auto* i: allInputs)
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

    w.setCentralWidget(mainFrame);
    w.resize(1150, 720);
    w.setWindowTitle("DateTimePicker Demo");
    w.show();
    return app.exec();
}

//--------------------------------------------------------------------------
