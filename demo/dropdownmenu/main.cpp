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

/** @file demo/dropdownmenu/main.cpp
*
*  Demo application of DropdownMenu.
*
*/

/****************************************************************************/

#include <QApplication>
#include <QMainWindow>
#include <QLabel>
#include <QPushButton>
#include <QPlainTextEdit>
#include <QScrollArea>
#include <QCursor>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/style.hpp>
#include <uise/desktop/icontextbutton.hpp>
#include <uise/desktop/dropdownmenu.hpp>

using namespace UISE_DESKTOP_NAMESPACE;

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

    auto* log=new QPlainTextEdit();
    log->setReadOnly(true);
    auto logMsg=[log](const QString& text)
    {
        log->appendPlainText(text);
    };

    auto menuIcon=[central](const QString& alias)
    {
        return Style::instance().svgIconLocator().icon(QString("DropdownMenu::%1").arg(alias),central);
    };

    // --- 1. plain clickable menu: File actions ---

    auto* fileRow=new QFrame(central);
    auto* fileRowL=Layout::horizontal(fileRow);
    rootLayout->addWidget(fileRow);

    fileRowL->addWidget(new QLabel(QStringLiteral("Clickable menu:")));

    auto* fileButton=new IconTextButton(QStringLiteral("File"),fileRow);
    fileRowL->addWidget(fileButton);
    fileRowL->addStretch(1);

    auto* fileMenu=new DropdownMenu(fileRow);
    fileMenu->setItems({
        MenuItem(1,QStringLiteral("Rename file"),menuIcon(QStringLiteral("rename"))),
        MenuItem::separator(),
        MenuItem(2,QStringLiteral("Remove"),menuIcon(QStringLiteral("delete")))
    });
    fileMenu->attachTo(fileButton);
    QObject::connect(
        fileMenu,
        &DropdownMenu::itemTriggered,
        log,
        [logMsg](int id)
        {
            logMsg(QString("File menu: item %1 triggered").arg(id));
        }
    );

    // --- 2. checkable menu, independent toggles: send-options ---

    auto* optionsRow=new QFrame(central);
    auto* optionsRowL=Layout::horizontal(optionsRow);
    rootLayout->addWidget(optionsRow);

    optionsRowL->addWidget(new QLabel(QStringLiteral("Checkable menu:")));

    auto* optionsButton=new IconTextButton(QStringLiteral("Options"),optionsRow);
    optionsRowL->addWidget(optionsButton);
    optionsRowL->addStretch(1);

    enum OptionId
    {
        HighQuality=1,
        SendAsDocuments=2,
        GroupItems=3
    };

    auto* optionsMenu=new DropdownMenu(optionsRow);
    optionsMenu->setItems({
        MenuItem::checkable(HighQuality,QStringLiteral("High quality"),true,menuIcon(QStringLiteral("highQuality"))),
        MenuItem::checkable(SendAsDocuments,QStringLiteral("Send as documents"),false,menuIcon(QStringLiteral("documents"))),
        MenuItem::checkable(GroupItems,QStringLiteral("Group items"),false,menuIcon(QStringLiteral("group")))
    });
    optionsMenu->attachTo(optionsButton);
    QObject::connect(
        optionsMenu,
        &DropdownMenu::itemToggled,
        log,
        [logMsg](int id, bool checked)
        {
            logMsg(QString("Options menu: item %1 -> %2").arg(id).arg(checked ? "checked" : "unchecked"));
        }
    );

    // --- 3. checkable menu, exclusive group: sort order ---

    auto* sortRow=new QFrame(central);
    auto* sortRowL=Layout::horizontal(sortRow);
    rootLayout->addWidget(sortRow);

    sortRowL->addWidget(new QLabel(QStringLiteral("Exclusive group:")));

    auto* sortButton=new IconTextButton(QStringLiteral("Sort by"),sortRow);
    sortRowL->addWidget(sortButton);
    sortRowL->addStretch(1);

    auto* sortMenu=new DropdownMenu(sortRow);
    auto sortItemName=MenuItem::checkable(1,QStringLiteral("Name"),true);
    sortItemName.group=0;
    auto sortItemDate=MenuItem::checkable(2,QStringLiteral("Date"),false);
    sortItemDate.group=0;
    auto sortItemSize=MenuItem::checkable(3,QStringLiteral("Size"),false);
    sortItemSize.group=0;
    sortMenu->setItems({sortItemName,sortItemDate,sortItemSize});
    sortMenu->setCloseOnCheckableActivation(true);
    sortMenu->attachTo(sortButton);
    QObject::connect(
        sortMenu,
        &DropdownMenu::itemToggled,
        log,
        [logMsg](int id, bool checked)
        {
            if (checked)
            {
                logMsg(QString("Sort menu: item %1 selected").arg(id));
            }
        }
    );

    // --- 4. right-edge pinned trigger, to exercise the horizontal corner flip ---

    auto* rightRow=new QFrame(central);
    auto* rightRowL=Layout::horizontal(rightRow);
    rootLayout->addWidget(rightRow);

    rightRowL->addWidget(new QLabel(QStringLiteral("Right-clamped (corner flip):")));
    rightRowL->addStretch(1);

    auto* rightButton=new IconTextButton(QStringLiteral("Menu"),rightRow);
    rightRowL->addWidget(rightButton);

    auto* rightMenu=new DropdownMenu(rightRow);
    rightMenu->setItems({
        MenuItem(1,QStringLiteral("A rather long item to force width")),
        MenuItem(2,QStringLiteral("Another item"))
    });
    rightMenu->attachTo(rightButton);

    // --- 5. cursor-anchored menu via popupAt() ---

    auto* cursorRow=new QFrame(central);
    auto* cursorRowL=Layout::horizontal(cursorRow);
    rootLayout->addWidget(cursorRow);

    cursorRowL->addWidget(new QLabel(QStringLiteral("Cursor-anchored (popupAt):")));

    auto* cursorButton=new QPushButton(QStringLiteral("Show at cursor"),cursorRow);
    cursorRowL->addWidget(cursorButton);
    cursorRowL->addStretch(1);

    auto* cursorMenu=new DropdownMenu(cursorRow);
    cursorMenu->setItems({
        MenuItem(1,QStringLiteral("Item at cursor A")),
        MenuItem(2,QStringLiteral("Item at cursor B"))
    });
    cursorMenu->setTriggerWidget(cursorButton);
    QObject::connect(
        cursorButton,
        &QPushButton::clicked,
        cursorMenu,
        [cursorMenu]()
        {
            cursorMenu->popupAt(QCursor::pos());
        }
    );

    // --- log ---

    rootLayout->addWidget(log,1);

    // --- theme toggle ---

    auto* controlsFrame=new QFrame(central);
    auto* cl=Layout::horizontal(controlsFrame);
    rootLayout->addWidget(controlsFrame);

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

    // --- 6. bottom-pinned trigger with a tall menu, to exercise the vertical flip ---

    auto* bottomRow=new QFrame(central);
    auto* bottomRowL=Layout::horizontal(bottomRow);
    rootLayout->addWidget(bottomRow);

    bottomRowL->addWidget(new QLabel(QStringLiteral("Bottom-pinned (vertical flip):")));
    bottomRowL->addStretch(1);

    auto* bottomButton=new IconTextButton(QStringLiteral("Tall menu"),bottomRow);
    bottomRowL->addWidget(bottomButton);

    auto* bottomMenu=new DropdownMenu(bottomRow);
    std::vector<MenuItem> tallItems;
    for (int i=1;i<=12;++i)
    {
        tallItems.push_back(MenuItem(i,QString("Row %1").arg(i)));
    }
    bottomMenu->setItems(std::move(tallItems));
    bottomMenu->attachTo(bottomButton);

    w.setCentralWidget(mainFrame);
    w.resize(700,420);
    w.setWindowTitle("DropdownMenu Demo");
    w.show();

    auto ret=app.exec();
    return ret;
}

//--------------------------------------------------------------------------
