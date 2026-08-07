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

/** @file demo/demomanager/demomanagerwindow.cpp
*
*  Defines DemoManagerWindow.
*
*/

/****************************************************************************/

#include <QCoreApplication>
#include <QFileInfo>
#include <QFrame>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QProcess>
#include <QScrollArea>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/style.hpp>
#include <uise/desktop/pushbutton.hpp>

#include "demoregistry.hpp"
#include "demomanagerwindow.hpp"

//--------------------------------------------------------------------------

DemoManagerWindow::DemoManagerWindow(QWidget* parent)
    : QMainWindow(parent),
      m_filterEdit(nullptr),
      m_countLabel(nullptr),
      m_listContent(nullptr),
      m_listLayout(nullptr)
{
    auto mainFrame=new QFrame(this);
    auto ml=Layout::vertical(mainFrame,false);
    ml->setContentsMargins(12,12,12,12);
    ml->setSpacing(8);

    // ---- header row: title, demo count, theme toggle ----
    auto headerFrame=new QFrame(mainFrame);
    auto hl=Layout::horizontal(headerFrame,false);
    hl->setSpacing(8);

    auto titleLabel=new QLabel("UISE Demos",headerFrame);
    auto titleFont=titleLabel->font();
    titleFont.setPointSize(titleFont.pointSize()+4);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    hl->addWidget(titleLabel);

    m_countLabel=new QLabel(headerFrame);
    hl->addWidget(m_countLabel);

    hl->addStretch(1);

    auto themeButton=new PushButton(headerFrame);
    themeButton->setCheckable(true);
    const bool startDark=Style::instance().checkDarkTheme();
    themeButton->setChecked(startDark);
    themeButton->setText(startDark ? "Dark" : "Light");
    connect(themeButton,&PushButton::toggled,this,
        [this,themeButton](bool dark)
        {
            themeButton->setText(dark ? "Dark" : "Light");
            toggleTheme(dark);
        }
    );
    hl->addWidget(themeButton);

    ml->addWidget(headerFrame,0);

    // ---- filter row ----
    m_filterEdit=new QLineEdit(mainFrame);
    m_filterEdit->setClearButtonEnabled(true);
    m_filterEdit->setPlaceholderText("Filter demos by title or description...");
    connect(m_filterEdit,&QLineEdit::textChanged,this,&DemoManagerWindow::applyFilter);
    ml->addWidget(m_filterEdit,0);

    // ---- scrollable demo list ----
    auto scrollArea=new QScrollArea(mainFrame);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);

    m_listContent=new QWidget(scrollArea);
    m_listLayout=Layout::vertical(m_listContent,false);
    m_listLayout->setSpacing(6);
    m_listLayout->setAlignment(Qt::AlignTop);
    scrollArea->setWidget(m_listContent);

    ml->addWidget(scrollArea,1);

    setCentralWidget(mainFrame);
    setWindowTitle("UISE Demos");
    resize(720,800);

    rebuildRows();
    applyFilter(QString());
}

//--------------------------------------------------------------------------

void DemoManagerWindow::rebuildRows()
{
    for (auto&& row : m_rows)
    {
        row.frame->deleteLater();
    }
    m_rows.clear();

    const QString appDir=QCoreApplication::applicationDirPath();
#ifdef Q_OS_WIN
    const QString exeSuffix=".exe";
#else
    const QString exeSuffix;
#endif

    for (const auto& demo : demoRegistry())
    {
        const auto executable=QString::fromUtf8(demo.executable);
        const auto title=QString::fromUtf8(demo.title);
        const auto description=QString::fromUtf8(demo.description);

        auto rowFrame=new QFrame(m_listContent);
        rowFrame->setObjectName("demoRow");
        rowFrame->setFrameShape(QFrame::StyledPanel);
        auto rl=Layout::horizontal(rowFrame,false);
        rl->setContentsMargins(8,8,8,8);
        rl->setSpacing(8);

        auto textFrame=new QFrame(rowFrame);
        auto tl=Layout::vertical(textFrame,false);
        tl->setSpacing(2);

        auto titleLabel=new QLabel(title,textFrame);
        auto titleFont=titleLabel->font();
        titleFont.setBold(true);
        titleLabel->setFont(titleFont);
        tl->addWidget(titleLabel);

        auto descLabel=new QLabel(description,textFrame);
        descLabel->setWordWrap(true);
        tl->addWidget(descLabel);

        rl->addWidget(textFrame,1);

        auto runButton=new PushButton("Run",rowFrame);
        rl->addWidget(runButton,0,Qt::AlignVCenter);

        const QString exePath=appDir+QLatin1Char('/')+executable+exeSuffix;
        if (!QFileInfo::exists(exePath))
        {
            runButton->setEnabled(false);
            runButton->setText("Not built");
            runButton->setToolTip(QString("%1 was not found next to the demo manager.").arg(exePath));
        }
        else
        {
            connect(runButton,&PushButton::clicked,this,
                [this,executable]()
                {
                    launchDemo(executable);
                }
            );
        }

        m_listLayout->addWidget(rowFrame);

        m_rows.push_back(DemoRow{executable,title,description,rowFrame});
    }
}

//--------------------------------------------------------------------------

void DemoManagerWindow::applyFilter(const QString& text)
{
    int visible=0;
    for (auto&& row : m_rows)
    {
        const bool matches=text.isEmpty()
            || row.title.contains(text,Qt::CaseInsensitive)
            || row.description.contains(text,Qt::CaseInsensitive)
            || row.executable.contains(text,Qt::CaseInsensitive);
        row.frame->setVisible(matches);
        if (matches)
        {
            ++visible;
        }
    }

    m_countLabel->setText(QString("%1 of %2 demos").arg(visible).arg(m_rows.size()));
}

//--------------------------------------------------------------------------

void DemoManagerWindow::launchDemo(const QString& executable)
{
    const QString appDir=QCoreApplication::applicationDirPath();
#ifdef Q_OS_WIN
    const QString exePath=appDir+QLatin1Char('/')+executable+".exe";
#else
    const QString exePath=appDir+QLatin1Char('/')+executable;
#endif

    if (!QProcess::startDetached(exePath,{},appDir))
    {
        QMessageBox::warning(this,"Failed to launch demo",QString("Could not start %1.").arg(exePath));
    }
}

//--------------------------------------------------------------------------

void DemoManagerWindow::toggleTheme(bool dark)
{
    Style::instance().setColorTheme(dark ? Style::DarkTheme : Style::LightTheme);
    Style::instance().applyStyleSheet(true);
    Style::repolishRecursive(this);
}

//--------------------------------------------------------------------------
