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

#include <memory>

#include <QApplication>
#include <QMainWindow>
#include <QLabel>
#include <QPushButton>
#include <QPlainTextEdit>
#include <QScrollArea>
#include <QCursor>
#include <QMetaEnum>
#include <QTimer>
#include <QStringList>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/style.hpp>
#include <uise/desktop/icontextbutton.hpp>
#include <uise/desktop/dropdownmenu.hpp>
#include <uise/desktop/htreelistitemtemplate.hpp>

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

    // --- 7. sectioned menu: section headers with indented subitems ---

    auto* sectionedRow=new QFrame(central);
    auto* sectionedRowL=Layout::horizontal(sectionedRow);
    rootLayout->addWidget(sectionedRow);

    sectionedRowL->addWidget(new QLabel(QStringLiteral("Sectioned menu:")));

    auto* sectionedButton=new IconTextButton(QStringLiteral("Add"),sectionedRow);
    sectionedRowL->addWidget(sectionedButton);
    sectionedRowL->addStretch(1);

    enum SectionedId
    {
        NewContact=1,
        WithUsername,
        WithLink,
        NewGroup,
        NewChannel
    };

    auto* sectionedMenu=new DropdownMenu(sectionedRow);
    sectionedMenu->setItems({
        MenuItem::section(NewContact,QStringLiteral("New contact")),
        MenuItem(WithUsername,QStringLiteral("I know a username")),
        MenuItem(WithLink,QStringLiteral("Use temporary code")),
        MenuItem::section(NewGroup,QStringLiteral("New group")),
        MenuItem::section(NewChannel,QStringLiteral("New channel"))
    });
    sectionedMenu->attachTo(sectionedButton);
    QObject::connect(
        sectionedMenu,
        &DropdownMenu::itemTriggered,
        log,
        [logMsg](int id)
        {
            logMsg(QString("Sectioned menu: item %1 triggered").arg(id));
        }
    );

    // --- 8. two-level submenu: mixed row kinds, icons, and a disabled submenu row ---

    auto* shareRow=new QFrame(central);
    auto* shareRowL=Layout::horizontal(shareRow);
    rootLayout->addWidget(shareRow);

    shareRowL->addWidget(new QLabel(QStringLiteral("Submenu (hover or click the chevron row):")));

    auto* shareButton=new IconTextButton(QStringLiteral("Share"),shareRow);
    shareRowL->addWidget(shareButton);
    shareRowL->addStretch(1);

    enum ShareId
    {
        Email=1,
        CopyLink,
        ExportSection,
        ImageFormatSubmenu,
        FormatPng,
        FormatJpeg,
        FormatSvg,
        MoreOptionsSubmenu,
        NothingHereYet
    };

    auto* shareMenu=new DropdownMenu(shareRow);
    shareMenu->setItems({
        MenuItem(Email,QStringLiteral("Email"),menuIcon(QStringLiteral("documents"))),
        MenuItem(CopyLink,QStringLiteral("Copy link"),menuIcon(QStringLiteral("copy"))),
        MenuItem::separator(),
        MenuItem::section(ExportSection,QStringLiteral("Export")),
        MenuItem::submenu(ImageFormatSubmenu,QStringLiteral("Image format"),{
            MenuItem(FormatPng,QStringLiteral("PNG")),
            MenuItem(FormatJpeg,QStringLiteral("JPEG")),
            MenuItem(FormatSvg,QStringLiteral("SVG"))
        }),
        // a disabled submenu row: never opens, no matter how long it is hovered or clicked
        MenuItem::submenu(MoreOptionsSubmenu,QStringLiteral("More options"),{
            MenuItem(NothingHereYet,QStringLiteral("Nothing here yet"))
        })
    });
    shareMenu->setItemEnabled(MoreOptionsSubmenu,false);
    shareMenu->attachTo(shareButton);
    QObject::connect(
        shareMenu,
        &DropdownMenu::itemTriggered,
        log,
        [logMsg](int id)
        {
            logMsg(QString("Share menu: item %1 triggered").arg(id));
        }
    );

    // --- 9. three-level nesting: root -> Shape -> Basic -> leaf ---

    auto* insertRow=new QFrame(central);
    auto* insertRowL=Layout::horizontal(insertRow);
    rootLayout->addWidget(insertRow);

    insertRowL->addWidget(new QLabel(QStringLiteral("Three-level nesting:")));

    auto* insertButton=new IconTextButton(QStringLiteral("Insert"),insertRow);
    insertRowL->addWidget(insertButton);
    insertRowL->addStretch(1);

    enum InsertId
    {
        Table=1,
        Image,
        ShapeSubmenu,
        Line,
        BasicSubmenu,
        Circle,
        Square,
        Triangle
    };

    auto* insertMenu=new DropdownMenu(insertRow);
    insertMenu->setItems({
        MenuItem(Table,QStringLiteral("Table")),
        MenuItem(Image,QStringLiteral("Image")),
        MenuItem::submenu(ShapeSubmenu,QStringLiteral("Shape"),{
            MenuItem(Line,QStringLiteral("Line")),
            MenuItem::submenu(BasicSubmenu,QStringLiteral("Basic"),{
                MenuItem(Circle,QStringLiteral("Circle")),
                MenuItem(Square,QStringLiteral("Square")),
                MenuItem(Triangle,QStringLiteral("Triangle"))
            })
        })
    });
    insertMenu->attachTo(insertButton);

    // the demo connects only to the ROOT menu -- itemTriggered (and itemToggled) from a
    // grandchild reaches this connection regardless of nesting depth, because each level of
    // DropdownMenu::ensureSubmenu() re-emits its own child's signal as its own
    QObject::connect(
        insertMenu,
        &DropdownMenu::itemTriggered,
        log,
        [logMsg](int id)
        {
            logMsg(QString("Insert menu: item %1 triggered (Circle/Square/Triangle are 3 levels deep)").arg(id));
        }
    );

    // closeRequested is NOT bubbled the way itemTriggered/itemToggled are -- each frame in a
    // chain emits its own independently, so this only observes the ROOT level's own closes
    // (e.g. the final Escape press, or an outside click, once no submenu is left open). Escape
    // pressed while Shape or Basic is open closes just that level instead, silently as far as
    // this particular connection is concerned -- open two or three levels and watch only the
    // LAST Escape press get logged here.
    QObject::connect(
        insertMenu,
        &DropdownFrame::closeRequested,
        log,
        [logMsg](DropdownFrame::CloseReason reason)
        {
            auto me=QMetaEnum::fromType<DropdownFrame::CloseReason>();
            logMsg(QString("Insert menu: root closeRequested (%1)").arg(me.valueToKey(static_cast<int>(reason))));
        }
    );

    // --- 10. checkable exclusive group inside a submenu ---

    auto* viewRow=new QFrame(central);
    auto* viewRowL=Layout::horizontal(viewRow);
    rootLayout->addWidget(viewRow);

    viewRowL->addWidget(new QLabel(QStringLiteral("Checkable group inside a submenu:")));

    auto* viewButton=new IconTextButton(QStringLiteral("View"),viewRow);
    viewRowL->addWidget(viewButton);

    enum ViewId
    {
        ShowRuler=1,
        ZoomSubmenu,
        ActualSize,
        FitPage,
        FitWidth
    };

    auto* viewMenu=new DropdownMenu(viewRow);
    // propagated to the Zoom submenu the first time it is created (see DropdownMenu::
    // ensureSubmenu()), so picking a zoom level closes the WHOLE menu, not just the submenu
    viewMenu->setCloseOnCheckableActivation(true);

    auto zoomActual=MenuItem::checkable(ActualSize,QStringLiteral("Actual size"),true);
    zoomActual.group=0;
    auto zoomFitPage=MenuItem::checkable(FitPage,QStringLiteral("Fit page"),false);
    zoomFitPage.group=0;
    auto zoomFitWidth=MenuItem::checkable(FitWidth,QStringLiteral("Fit width"),false);
    zoomFitWidth.group=0;

    viewMenu->setItems({
        MenuItem::checkable(ShowRuler,QStringLiteral("Show ruler"),true),
        MenuItem::submenu(ZoomSubmenu,QStringLiteral("Zoom"),{zoomActual,zoomFitPage,zoomFitWidth})
    });
    viewMenu->attachTo(viewButton);
    QObject::connect(
        viewMenu,
        &DropdownMenu::itemToggled,
        log,
        [logMsg](int id, bool checked)
        {
            // bubbles up from the Zoom submenu exactly like itemTriggered does; the checked
            // state is also written back into viewMenu's own copy of the nested descriptor, so
            // it survives the submenu being closed and re-opened
            logMsg(QString("View menu: item %1 -> %2").arg(id).arg(checked ? "checked" : "unchecked"));
        }
    );

    viewRowL->addStretch(1);
    auto* setFitWidthButton=new QPushButton(QStringLiteral("Select 'Fit width' (menu closed)"),viewRow);
    viewRowL->addWidget(setFitWidthButton);
    QObject::connect(
        setFitWidthButton,
        &QPushButton::clicked,
        viewMenu,
        [viewMenu,logMsg]()
        {
            // demonstrates the recursive mutators: FitWidth/ActualSize belong to the Zoom
            // SUBMENU, not to viewMenu's own top level, yet setItemChecked() finds and updates
            // them (and, if the Zoom submenu has already been opened once, its live row too) --
            // see DropdownMenu_p::findItemRecursive() and the per-cached-child forwarding in
            // every setItem*() mutator
            viewMenu->setItemChecked(ActualSize,false);
            viewMenu->setItemChecked(FitWidth,true);
            logMsg(QStringLiteral("View menu: 'Fit width' selected programmatically while closed"));
        }
    );

    // --- 11. right-clamped trigger with a submenu: exercises the submenu's own left flip ---

    auto* alignRow=new QFrame(central);
    auto* alignRowL=Layout::horizontal(alignRow);
    rootLayout->addWidget(alignRow);

    alignRowL->addWidget(new QLabel(QStringLiteral("Right-clamped submenu (left flip):")));
    alignRowL->addStretch(1);

    auto* alignButton=new IconTextButton(QStringLiteral("Align"),alignRow);
    alignRowL->addWidget(alignButton);

    enum AlignId
    {
        AlignLeft=1,
        AlignCenter,
        DistributeSubmenu,
        DistributeHorizontally,
        DistributeVertically
    };

    auto* alignMenu=new DropdownMenu(alignRow);
    alignMenu->setItems({
        MenuItem(AlignLeft,QStringLiteral("Align left")),
        MenuItem(AlignCenter,QStringLiteral("Align center")),
        MenuItem::submenu(DistributeSubmenu,QStringLiteral("Distribute"),{
            MenuItem(DistributeHorizontally,QStringLiteral("Horizontally")),
            MenuItem(DistributeVertically,QStringLiteral("Vertically"))
        })
    });
    alignMenu->attachTo(alignButton);
    QObject::connect(
        alignMenu,
        &DropdownMenu::itemTriggered,
        log,
        [logMsg](int id)
        {
            logMsg(QString("Align menu: item %1 triggered").arg(id));
        }
    );

    // --- 12. bottom-pinned trigger with a tall submenu: exercises the submenu's own bottom clip ---

    auto* recentRow=new QFrame(central);
    auto* recentRowL=Layout::horizontal(recentRow);
    rootLayout->addWidget(recentRow);

    recentRowL->addWidget(new QLabel(QStringLiteral("Bottom-pinned submenu (tall, ~20 rows):")));
    recentRowL->addStretch(1);

    auto* recentButton=new IconTextButton(QStringLiteral("File"),recentRow);
    recentRowL->addWidget(recentButton);

    enum RecentId
    {
        NewFile=1,
        OpenRecentSubmenu,
        RecentFirstRow
    };

    std::vector<MenuItem> recentFiles;
    for (int i=0;i<20;++i)
    {
        recentFiles.push_back(MenuItem(RecentFirstRow+i,QString("document-%1.txt").arg(i+1)));
    }

    auto* recentMenu=new DropdownMenu(recentRow);
    recentMenu->setItems({
        MenuItem(NewFile,QStringLiteral("New")),
        MenuItem::submenu(OpenRecentSubmenu,QStringLiteral("Open recent"),std::move(recentFiles))
    });
    recentMenu->attachTo(recentButton);
    QObject::connect(
        recentMenu,
        &DropdownMenu::itemTriggered,
        log,
        [logMsg](int id)
        {
            logMsg(QString("File menu (bottom-pinned): item %1 triggered").arg(id));
        }
    );

    // --- 13. frame submenu: a row that opens a host-provided frame instead of a child menu ---
    //
    // The shape a "Read by N" row of a chat message needs: the submenu content is a custom widget
    // that is filled asynchronously after the row opens it, and the row's own text is updated
    // once the data arrives.

    auto* audienceRow=new QFrame(central);
    auto* audienceRowL=Layout::horizontal(audienceRow);
    rootLayout->addWidget(audienceRow);

    audienceRowL->addWidget(new QLabel(QStringLiteral("Frame submenu (content loads after 400 ms):")));

    auto* audienceButton=new IconTextButton(QStringLiteral("Message"),audienceRow);
    audienceRowL->addWidget(audienceButton);
    audienceRowL->addStretch(1);

    enum AudienceId
    {
        AudienceCopy=1,
        AudienceReadBy,
        AudienceInfo
    };

    // informational: normal colours, but no hover, no click, and the menu stays open
    MenuItem audienceInfoItem(AudienceInfo,QStringLiteral("Read at 21:37"));
    audienceInfoItem.isInert=true;

    auto* audienceMenu=new DropdownMenu(audienceRow);
    audienceMenu->setItems({
        MenuItem(AudienceCopy,QStringLiteral("Copy")),
        MenuItem::frameSubmenu(AudienceReadBy,QStringLiteral("Read by...")),
        MenuItem::separator(),
        audienceInfoItem
    });
    audienceMenu->attachTo(audienceButton);

    // owned by the host, created once; the menu only borrows it
    auto* audienceFrame=new DropdownFrame(audienceRow);
    // a bare DropdownFrame is fully transparent (and so click-through): take the menu's look
    audienceFrame->setObjectName("dropdownPanel");
    QObject::connect(
        audienceMenu,
        &DropdownMenu::submenuAboutToShow,
        audienceFrame,
        [audienceMenu,audienceFrame](int id)
        {
            if (id!=AudienceReadBy)
            {
                return;
            }

            // a no-op after the first opening
            audienceMenu->setSubmenuFrame(id,audienceFrame);

            auto* loading=new QLabel(QStringLiteral("Loading..."));
            loading->setMargin(8);
            audienceFrame->setContent(loading);

            QTimer::singleShot(
                400,
                audienceFrame,
                [audienceMenu,audienceFrame]()
                {
                    auto* list=new QFrame();
                    auto* listL=Layout::vertical(list);
                    const QStringList names{QStringLiteral("Alice  10:41"),QStringLiteral("Bob  10:43"),QStringLiteral("Carol  10:50")};
                    for (const auto& name : names)
                    {
                        auto* label=new QLabel(name);
                        label->setMargin(6);
                        listL->addWidget(label);
                    }
                    audienceFrame->setContent(list);
                    audienceMenu->setItemText(AudienceReadBy,QString("Read by %1").arg(names.size()));
                    // the frame is open and was measured around "Loading..."
                    audienceFrame->remeasure();
                }
            );
        }
    );
    QObject::connect(
        audienceMenu,
        &DropdownMenu::itemTriggered,
        log,
        [logMsg](int id)
        {
            logMsg(QString("Message menu: item %1 triggered").arg(id));
        }
    );

    // --- 14. row hints and the HTreeListItemMenu builder (the shape of a notification-settings menu) ---
    //
    // Hint: a right-aligned secondary label per row (MenuItem::hint, setItemHint()). Builder: what a
    // list row's context menu is filled through, with hints and nested submenus; handlers are
    // shared across the levels, so the log shows which one ran for every leaf.

    auto* hintRow=new QFrame(central);
    auto* hintRowL=Layout::horizontal(hintRow);
    rootLayout->addWidget(hintRow);

    hintRowL->addWidget(new QLabel(QStringLiteral("Hints:")));

    auto* hintButton=new IconTextButton(QStringLiteral("Notifications"),hintRow);
    hintRowL->addWidget(hintButton);

    enum HintId
    {
        HintUnmute=1,
        HintSoundOff,
        HintCheckable
    };

    auto* hintMenu=new DropdownMenu(hintRow);
    MenuItem unmuteItem(HintUnmute,QStringLiteral("Unmute"),menuIcon(QStringLiteral("copy")));
    unmuteItem.hint=QStringLiteral("25 min");
    MenuItem soundOffItem(HintSoundOff,QStringLiteral("Disable sound"));
    auto checkableHintItem=MenuItem::checkable(HintCheckable,QStringLiteral("Checkable row"),true);
    checkableHintItem.hint=QStringLiteral("on");
    hintMenu->setItems({unmuteItem,soundOffItem,MenuItem::separator(),checkableHintItem});
    hintMenu->attachTo(hintButton);
    QObject::connect(
        hintMenu,
        &DropdownMenu::itemTriggered,
        log,
        [logMsg](int id)
        {
            logMsg(QString("Hint menu: item %1 triggered").arg(id));
        }
    );

    // changes hints while the menu is closed or open
    auto hintStep=std::make_shared<int>(0);
    auto* hintChange=new QPushButton(QStringLiteral("Change hints"),hintRow);
    hintRowL->addWidget(hintChange);
    QObject::connect(
        hintChange,
        &QPushButton::clicked,
        hintMenu,
        [hintMenu,hintStep]()
        {
            static const QStringList values{QStringLiteral("24 min"),QStringLiteral("1 h"),QStringLiteral("a much longer hint"),QString()};
            const auto value=values.at((*hintStep)++ % values.size());
            hintMenu->setItemHint(HintUnmute,value);
            hintMenu->setItemHint(HintSoundOff,value);
        }
    );
    hintRowL->addStretch(1);

    auto* builderRow=new QFrame(central);
    auto* builderRowL=Layout::horizontal(builderRow);
    rootLayout->addWidget(builderRow);

    builderRowL->addWidget(new QLabel(QStringLiteral("HTreeListItemMenu builder (submenu + hints):")));

    auto* builderButton=new IconTextButton(QStringLiteral("Row menu"),builderRow);
    builderRowL->addWidget(builderButton);
    builderRowL->addStretch(1);

    // rebuilt on every popup, like a list row does; the builder owns the handlers
    auto builder=std::make_shared<HTreeListItemMenu>();
    auto* builderMenu=new DropdownMenu(builderRow);
    // the items are built on the trigger's click, so that connection must come BEFORE attachTo()
    // (whose own clicked handler opens the menu)
    QObject::connect(
        builderButton,
        &IconTextButton::clicked,
        builderMenu,
        [builder,builderMenu,logMsg]()
        {
            builder->clear();
            builder->addItem(QStringLiteral("Unmute"),[logMsg]() { logMsg(QStringLiteral("builder: Unmute")); },QStringLiteral("25 min"));
            builder->addItem(QStringLiteral("Disable sound"),[logMsg]() { logMsg(QStringLiteral("builder: Disable sound")); });
            auto& mute=builder->addSubmenu(QStringLiteral("Mute"));
            mute.addItem(QStringLiteral("For 15 minutes"),[logMsg]() { logMsg(QStringLiteral("builder: mute 15 min")); });
            mute.addItem(QStringLiteral("For 1 hour"),[logMsg]() { logMsg(QStringLiteral("builder: mute 1 h")); });
            auto& longer=mute.addSubmenu(QStringLiteral("Longer"));
            longer.addItem(QStringLiteral("For 3 months"),[logMsg]() { logMsg(QStringLiteral("builder: mute 3 months")); });
            longer.addItem(QStringLiteral("Forever"),[logMsg]() { logMsg(QStringLiteral("builder: mute forever")); });
            builder->addSubmenu(QStringLiteral("Empty submenu (shown disabled)"));
            builderMenu->setItems(builder->items());
        }
    );
    builderMenu->attachTo(builderButton);
    QObject::connect(
        builderMenu,
        &DropdownMenu::itemTriggered,
        log,
        [builder](int id)
        {
            auto handler=builder->handler(id);
            if (handler)
            {
                handler();
            }
        }
    );

    w.setCentralWidget(mainFrame);
    w.resize(700,420);
    w.setWindowTitle("DropdownMenu Demo");
    w.show();

    auto ret=app.exec();
    return ret;
}

//--------------------------------------------------------------------------
