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

/** @file demo/fastswitchbutton/main.cpp
*
*  Demo application of FastSwitchButton.
*
*/

/****************************************************************************/

#include <QApplication>
#include <QMainWindow>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QComboBox>
#include <QPlainTextEdit>
#include <QScrollArea>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/utils/destroywidget.hpp>
#include <uise/desktop/style.hpp>
#include <uise/desktop/icontextbutton.hpp>
#include <uise/desktop/avatarbutton.hpp>
#include <uise/desktop/navigationbar.hpp>
#include <uise/desktop/fastswitchbutton.hpp>

using namespace UISE_DESKTOP_NAMESPACE;

//--------------------------------------------------------------------------

/**
 * @brief Demo subclass exercising every FastSwitchButton hook.
 *
 * The extra widget is an AvatarButton rather than the default IconTextButton, to prove
 * that createExtraWidget() lets subclasses pick a different widget class. Every hook logs
 * its invocation to a QPlainTextEdit so the create-once/fill-per-open lifecycle described
 * in fastswitchbutton.hpp is directly observable while playing with the demo controls.
 */
class DemoFastSwitchButton : public FastSwitchButton
{
    public:

        using FastSwitchButton::FastSwitchButton;

        void setLog(QPlainTextEdit* log) noexcept
        {
            m_log=log;
        }

        void setRowCount(int count) noexcept
        {
            m_rowCount=count;
        }

    protected:

        QWidget* createExtraWidget(QWidget* parent) override
        {
            logMsg(QStringLiteral("createExtraWidget (AvatarButton)"));
            auto* btn=new AvatarButton(parent);
            btn->setObjectName("extraWidget");
            return btn;
        }

        void fillExtraWidget(QWidget* widget) override
        {
            ++m_fillCount;
            logMsg(QString("fillExtraWidget #%1").arg(m_fillCount));

            if (auto* btn=qobject_cast<AvatarButton*>(widget))
            {
                btn->setText(m_lastActivated.isEmpty() ? QStringLiteral("Go to chats") : m_lastActivated);
            }
        }

        void clearExtraWidget(QWidget* widget) override
        {
            ++m_clearCount;
            logMsg(QString("clearExtraWidget #%1 (open fills: %2)").arg(m_clearCount).arg(m_fillCount-m_clearCount));

            if (auto* btn=qobject_cast<AvatarButton*>(widget))
            {
                btn->setText(QString());
            }
        }

        QWidget* createDropdownContent(QWidget* parent) override
        {
            logMsg(QStringLiteral("createDropdownContent"));

            auto* content=new QFrame(parent);
            content->setObjectName("dropdownContent");
            auto* l=Layout::vertical(content);

            m_goToChats=new IconTextButton(
                QStringLiteral("Go to chats"),
                Style::instance().svgIconLocator().icon(QStringLiteral("FastSwitchButton::chats"),content),
                content
            );
            l->addWidget(m_goToChats);
            connect(
                m_goToChats,
                &IconTextButton::clicked,
                this,
                [this]()
                {
                    m_lastActivated.clear();
                    logMsg(QStringLiteral("activated: Go to chats"));
                    notifyActivated(m_goToChats);
                }
            );

            m_separator=new QFrame(content);
            m_separator->setObjectName("separator");
            l->addWidget(m_separator);

            m_rowsHost=new QFrame(content);
            Layout::vertical(m_rowsHost);
            l->addWidget(m_rowsHost);

            return content;
        }

        void fillDropdownContent(QWidget*) override
        {
            logMsg(QString("fillDropdownContent: %1 rows").arg(m_rowCount));

            const auto rows=m_rowsHost->findChildren<QWidget*>(QString(),Qt::FindDirectChildrenOnly);
            for (auto* row : rows)
            {
                destroyWidget(row);
            }

            for (int i=0;i<m_rowCount;++i)
            {
                auto title=QString("Recent chat %1").arg(i+1);
                auto* row=new IconTextButton(title,m_rowsHost);
                m_rowsHost->layout()->addWidget(row);
                connect(
                    row,
                    &IconTextButton::clicked,
                    this,
                    [this,title,row]()
                    {
                        m_lastActivated=title;
                        logMsg(QString("activated: %1").arg(title));
                        notifyActivated(row);
                    }
                );
            }
        }

        void clearDropdownContent(QWidget*) override
        {
            logMsg(QStringLiteral("clearDropdownContent"));

            const auto rows=m_rowsHost->findChildren<QWidget*>(QString(),Qt::FindDirectChildrenOnly);
            for (auto* row : rows)
            {
                destroyWidget(row);
            }
        }

        void onStateChanged(State state) override
        {
            QString name;
            switch (state)
            {
                case (State::Normal): name=QStringLiteral("Normal"); break;
                case (State::Hovered): name=QStringLiteral("Hovered"); break;
                case (State::Dropdown): name=QStringLiteral("Dropdown"); break;
            }
            logMsg(QString("state -> %1").arg(name));
        }

    private:

        void logMsg(const QString& text)
        {
            if (m_log!=nullptr)
            {
                m_log->appendPlainText(text);
            }
        }

        QPlainTextEdit* m_log=nullptr;
        int m_rowCount=3;
        int m_fillCount=0;
        int m_clearCount=0;
        QString m_lastActivated;

        IconTextButton* m_goToChats=nullptr;
        QFrame* m_separator=nullptr;
        QFrame* m_rowsHost=nullptr;
};

//--------------------------------------------------------------------------

int main(int argc, char *argv[])
{
    QApplication app(argc,argv);

    Style::instance().applyStyleSheet();

    QMainWindow w;

    auto* mainFrame=new QScrollArea();
    mainFrame->setWidgetResizable(true);
    auto* central=new QFrame(mainFrame);
    auto* rootLayout=Layout::vertical(central,false);
    mainFrame->setWidget(central);

    // --- a real navigation bar, mirroring Back/Home/.../FastSwitchButton/Lock/Quit ---

    auto* navbar=new NavigationBar(central);
    rootLayout->addWidget(navbar);

    auto makeNavIconButton=[navbar](const QString& iconAlias, const QString& tooltip)
    {
        auto* btn=new IconTextButton(
            Style::instance().svgIconLocator().icon(QString("FastSwitchButton::%1").arg(iconAlias),navbar),
            navbar,
            IconTextButton::IconPosition::BeforeText
        );
        btn->setText(QString());
        btn->setToolTip(tooltip);
        return btn;
    };

    navbar->addLeadingWidget(makeNavIconButton(QStringLiteral("back"),QStringLiteral("Back")));
    navbar->addLeadingWidget(makeNavIconButton(QStringLiteral("home"),QStringLiteral("Home")));

    auto* log=new QPlainTextEdit();
    log->setReadOnly(true);

    auto* fastSwitch=new DemoFastSwitchButton(
        Style::instance().svgIconLocator().icon(QStringLiteral("FastSwitchButton::chats"),navbar),
        navbar
    );
    fastSwitch->setLog(log);
    navbar->addLeadingWidget(fastSwitch);

    navbar->addLeadingWidget(makeNavIconButton(QStringLiteral("lock"),QStringLiteral("Lock")));

    QObject::connect(
        fastSwitch,
        &FastSwitchButton::activated,
        log,
        [log](QWidget*)
        {
            log->appendPlainText(QStringLiteral("activated() signal emitted"));
        }
    );

    // --- a second instance pinned to the right edge, to exercise top-right corner clamping ---

    auto* rightBar=new QFrame(central);
    auto* rightLayout=Layout::horizontal(rightBar);
    rootLayout->addWidget(rightBar);

    rightLayout->addWidget(new QLabel(QStringLiteral("Right-clamped instance:")));
    rightLayout->addStretch(1);

    auto* fastSwitchRight=new DemoFastSwitchButton(
        Style::instance().svgIconLocator().icon(QStringLiteral("FastSwitchButton::chats"),rightBar),
        rightBar
    );
    fastSwitchRight->setLog(log);
    rightLayout->addWidget(fastSwitchRight);

    // --- log ---

    rootLayout->addWidget(log,1);

    // --- controls ---

    auto* controlsFrame=new QFrame(central);
    auto* cl=Layout::horizontal(controlsFrame);
    rootLayout->addWidget(controlsFrame);

    auto bindBoth=[fastSwitch,fastSwitchRight](auto&& fn)
    {
        fn(fastSwitch);
        fn(fastSwitchRight);
    };

    cl->addWidget(new QLabel(QStringLiteral("Extra slide, ms:")));
    auto* extraDuration=new QSpinBox();
    extraDuration->setRange(0,3000);
    extraDuration->setValue(fastSwitch->extraSlideDurationMs());
    cl->addWidget(extraDuration);
    QObject::connect(
        extraDuration,
        &QSpinBox::valueChanged,
        fastSwitch,
        [bindBoth](int val)
        {
            bindBoth([val](DemoFastSwitchButton* b){ b->setExtraSlideDurationMs(val); });
        }
    );

    cl->addWidget(new QLabel(QStringLiteral("Dropdown, ms:")));
    auto* dropDuration=new QSpinBox();
    dropDuration->setRange(0,3000);
    dropDuration->setValue(fastSwitch->dropdownAnimationDurationMs());
    cl->addWidget(dropDuration);
    QObject::connect(
        dropDuration,
        &QSpinBox::valueChanged,
        fastSwitch,
        [bindBoth](int val)
        {
            bindBoth([val](DemoFastSwitchButton* b){ b->setDropdownAnimationDurationMs(val); });
        }
    );

    cl->addWidget(new QLabel(QStringLiteral("Easing:")));
    auto* easing=new QComboBox();
    easing->addItem(QStringLiteral("Linear"),static_cast<int>(QEasingCurve::Linear));
    easing->addItem(QStringLiteral("OutCubic"),static_cast<int>(QEasingCurve::OutCubic));
    easing->addItem(QStringLiteral("InOutSine"),static_cast<int>(QEasingCurve::InOutSine));
    easing->addItem(QStringLiteral("OutBounce"),static_cast<int>(QEasingCurve::OutBounce));
    easing->setCurrentIndex(1);
    cl->addWidget(easing);
    QObject::connect(
        easing,
        &QComboBox::currentIndexChanged,
        fastSwitch,
        [bindBoth,easing](int idx)
        {
            auto type=easing->itemData(idx).toInt();
            bindBoth([type](DemoFastSwitchButton* b)
            {
                b->setExtraEasingCurveType(type);
                b->setDropdownEasingCurveType(type);
            });
        }
    );

    cl->addWidget(new QLabel(QStringLiteral("Rows:")));
    auto* rows=new QSpinBox();
    rows->setRange(0,20);
    rows->setValue(3);
    cl->addWidget(rows);
    QObject::connect(
        rows,
        &QSpinBox::valueChanged,
        fastSwitch,
        [bindBoth](int val)
        {
            bindBoth([val](DemoFastSwitchButton* b){ b->setRowCount(val); });
        }
    );

    auto* themeButton=new QPushButton(QStringLiteral("Toggle theme"));
    cl->addWidget(themeButton);
    QObject::connect(
        themeButton,
        &QPushButton::clicked,
        &app,
        []()
        {
            auto mode=Style::instance().styleSheetMode();
            auto newMode=(mode==Style::StyleSheetMode::Dark)
                            ? Style::StyleSheetMode::Light
                            : Style::StyleSheetMode::Dark;
            Style::instance().setStyleSheetMode(newMode);
            Style::instance().applyStyleSheet(true);
        }
    );

    cl->addStretch(1);

    w.setCentralWidget(mainFrame);
    w.resize(900,600);
    w.setWindowTitle("FastSwitchButton Demo");
    w.show();

    auto ret=app.exec();
    return ret;
}

//--------------------------------------------------------------------------
