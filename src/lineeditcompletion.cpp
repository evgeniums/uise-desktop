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

/** @file uise/desktop/src/lineeditcompletion.cpp
*
*  Defines LineEditCompletion.
*
*/

/****************************************************************************/

#include <algorithm>
#include <utility>

#include <QEvent>
#include <QFrame>
#include <QKeyEvent>
#include <QLineEdit>
#include <QListWidget>
#include <QTimer>

#include <uise/desktop/lineeditcompletion.hpp>

namespace uise {

namespace {

bool isWordChar(QChar ch)
{
    return ch.isLetterOrNumber() || ch==QLatin1Char('_');
}

} // anonymous namespace

//--------------------------------------------------------------------------

LineEditCompletion::LineEditCompletion(QLineEdit* edit, QWidget* listParent, QChar trigger)
    : QObject(listParent),
      m_edit(edit),
      m_listParent(listParent),
      m_trigger(trigger),
      m_delayMs(DefaultDelayMs),
      m_timer(new QTimer(this)),
      m_list(new QListWidget(listParent))
{
    m_timer->setSingleShot(true);
    connect(m_timer,&QTimer::timeout,this,&LineEditCompletion::ask);

    m_list->setObjectName("completionList");
    m_list->setFrameShape(QFrame::NoFrame);
    // the line keeps the keyboard; a click on a row picks it and does not move the focus
    m_list->setFocusPolicy(Qt::NoFocus);
    m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    m_list->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_list->setUniformItemSizes(true);
    m_list->setMouseTracking(true);
    m_list->hide();
    connect(m_list.data(),&QListWidget::itemClicked,this,
        [this](QListWidgetItem* item)
        {
            accept(m_list->row(item));
        }
    );

    connect(edit,&QLineEdit::textEdited,this,&LineEditCompletion::onTextEdited);
    connect(edit,&QLineEdit::cursorPositionChanged,this,
        [this](int, int)
        {
            onCursorPositionChanged();
        }
    );
    // a text set from outside (the clear button of the line) may take the word away
    connect(edit,&QLineEdit::textChanged,this,
        [this](const QString&)
        {
            onCursorPositionChanged();
        }
    );
}

//--------------------------------------------------------------------------

LineEditCompletion::~LineEditCompletion()
{
    if (!m_edit.isNull())
    {
        m_edit->removeEventFilter(this);
    }
}

//--------------------------------------------------------------------------

void LineEditCompletion::setSource(SourceCb source)
{
    m_source=std::move(source);
    if (!m_source)
    {
        close();
    }
}

//--------------------------------------------------------------------------

void LineEditCompletion::setDelayMs(int delay) noexcept
{
    m_delayMs=std::max(delay,0);
}

//--------------------------------------------------------------------------

bool LineEditCompletion::isOpen() const
{
    return m_open;
}

//--------------------------------------------------------------------------

QStringList LineEditCompletion::candidates() const
{
    QStringList result;
    if (m_open && !m_list.isNull())
    {
        for (int i=0;i<m_list->count();++i)
        {
            result.push_back(m_list->item(i)->text());
        }
    }
    return result;
}

//--------------------------------------------------------------------------

int LineEditCompletion::currentRow() const
{
    return (m_open && !m_list.isNull()) ? m_list->currentRow() : -1;
}

//--------------------------------------------------------------------------

LineEditCompletion::Word LineEditCompletion::wordAtCaret() const
{
    Word word;
    if (m_edit.isNull())
    {
        return word;
    }

    const QString text=m_edit->text();
    const int caret=m_edit->cursorPosition();

    int begin=caret;
    while (begin>0 && isWordChar(text.at(begin-1)))
    {
        --begin;
    }
    if (begin==0 || text.at(begin-1)!=m_trigger)
    {
        return word;
    }
    const int start=begin-1;
    // glued to a letter or a digit before it: "abc#tr" is not a tag
    if (start>0 && text.at(start-1).isLetterOrNumber())
    {
        return word;
    }

    int end=caret;
    while (end<text.size() && isWordChar(text.at(end)))
    {
        ++end;
    }

    word.start=start;
    word.end=end;
    word.text=text.mid(start,caret-start);
    return word;
}

//--------------------------------------------------------------------------

void LineEditCompletion::onTextEdited()
{
    if (!m_source)
    {
        return;
    }

    if (wordAtCaret().start<0)
    {
        close();
        return;
    }

    // what was asked before is overtaken
    ++m_generation;
    if (m_delayMs<=0)
    {
        ask();
        return;
    }
    m_timer->start(m_delayMs);
}

//--------------------------------------------------------------------------

void LineEditCompletion::onCursorPositionChanged()
{
    if (!m_open)
    {
        return;
    }
    // typing goes on in the same word, and moving within it too; leaving it ends the list
    if (wordAtCaret().start!=m_wordStart)
    {
        close();
    }
}

//--------------------------------------------------------------------------

void LineEditCompletion::ask()
{
    m_timer->stop();
    if (!m_source)
    {
        return;
    }

    const auto word=wordAtCaret();
    if (word.start<0)
    {
        close();
        return;
    }

    const auto generation=++m_generation;
    const QString asked=word.text;
    QPointer<LineEditCompletion> self(this);
    m_source(
        asked,
        [self,generation,asked](const QStringList& candidates)
        {
            if (!self.isNull() && self->m_generation==generation)
            {
                self->show(candidates,asked);
            }
        }
    );
}

//--------------------------------------------------------------------------

void LineEditCompletion::show(const QStringList& candidates, const QString& forWord)
{
    if (m_edit.isNull() || m_list.isNull())
    {
        return;
    }
    const auto word=wordAtCaret();
    // the answer is for a word that is not the one under the caret any more
    if (word.start<0 || word.text!=forWord)
    {
        return;
    }

    QStringList items;
    for (const auto& candidate : candidates)
    {
        if (!candidate.isEmpty() && !items.contains(candidate))
        {
            items.push_back(candidate);
            if (items.size()>=MaxRows)
            {
                break;
            }
        }
    }

    // nothing to offer, or only what is typed already: the word is finished, Enter keeps its own meaning
    if (items.isEmpty() || (items.size()==1 && items.front().compare(forWord,Qt::CaseInsensitive)==0))
    {
        close();
        return;
    }

    m_list->clear();
    m_list->addItems(items);
    m_list->setCurrentRow(0);
    m_open=true;
    m_wordStart=word.start;
    place();
    m_list->show();
    m_list->raise();

    // installed again on every opening: a filter installed last sees an event first, and this one must see the keys of the
    // line before whatever else watches it (a page that closes on Escape)
    m_edit->installEventFilter(this);
}

//--------------------------------------------------------------------------

void LineEditCompletion::place()
{
    if (m_edit.isNull() || m_listParent.isNull() || m_list.isNull() || m_list->count()==0)
    {
        return;
    }

    const int rowHeight=std::max(m_list->sizeHintForRow(0),m_list->fontMetrics().height()+8);
    const int frame=2*m_list->frameWidth();
    int height=m_list->count()*rowHeight+frame;

    const QPoint topLeft=m_listParent->mapFromGlobal(m_edit->mapToGlobal(QPoint(0,m_edit->height()+2)));
    // never beyond the bottom of the widget it is placed in (what does not fit scrolls); at least one row
    const int available=m_listParent->height()-topLeft.y();
    if (available>0)
    {
        height=std::min(height,std::max(available,rowHeight+frame));
    }
    m_list->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_list->setGeometry(topLeft.x(),topLeft.y(),m_edit->width(),height);
}

//--------------------------------------------------------------------------

void LineEditCompletion::close()
{
    ++m_generation;
    m_timer->stop();
    m_open=false;
    m_wordStart=-1;
    if (!m_edit.isNull())
    {
        m_edit->removeEventFilter(this);
    }
    if (!m_list.isNull())
    {
        m_list->hide();
        m_list->clear();
    }
}

//--------------------------------------------------------------------------

void LineEditCompletion::moveSelection(int delta)
{
    if (!m_open || m_list.isNull() || m_list->count()==0)
    {
        return;
    }
    const int count=m_list->count();
    int next=(m_list->currentRow()+delta)%count;
    if (next<0)
    {
        next+=count;
    }
    m_list->setCurrentRow(next);
}

//--------------------------------------------------------------------------

bool LineEditCompletion::acceptCurrent()
{
    if (!m_open || m_list.isNull() || m_list->currentRow()<0)
    {
        return false;
    }
    accept(m_list->currentRow());
    return true;
}

//--------------------------------------------------------------------------

void LineEditCompletion::accept(int row)
{
    if (!m_open || m_edit.isNull() || m_list.isNull() || row<0 || row>=m_list->count())
    {
        return;
    }

    const QString candidate=m_list->item(row)->text();
    const auto word=wordAtCaret();
    if (word.start<0)
    {
        close();
        return;
    }

    // the whole word goes, the part after the caret too; a space after it when it was the end of the text, to go on typing
    QString text=m_edit->text();
    QString replacement=candidate;
    if (word.end>=text.size())
    {
        replacement+=QLatin1Char(' ');
    }
    text.replace(word.start,word.end-word.start,replacement);

    // closed first: the new text must not be asked about (setText() emits no textEdited() anyway)
    close();
    m_edit->setText(text);
    m_edit->setCursorPosition(word.start+replacement.size());
    emit accepted(candidate);
}

//--------------------------------------------------------------------------

bool LineEditCompletion::eventFilter(QObject* watched, QEvent* event)
{
    if (watched!=m_edit.data() || !m_open)
    {
        return QObject::eventFilter(watched,event);
    }

    switch (event->type())
    {
        case QEvent::ShortcutOverride:
        {
            // Escape is the list's while it is open: a shortcut bound to the key (close the page, close the chat) does not fire
            auto* keyEvent=static_cast<QKeyEvent*>(event);
            if (keyEvent->key()==Qt::Key_Escape && keyEvent->modifiers()==Qt::NoModifier)
            {
                event->accept();
                return true;
            }
            break;
        }

        case QEvent::KeyPress:
        {
            auto* keyEvent=static_cast<QKeyEvent*>(event);
            // macOS reports the arrow keys with the keypad modifier
            if ((keyEvent->modifiers() & ~Qt::KeypadModifier)!=Qt::NoModifier)
            {
                break;
            }
            switch (keyEvent->key())
            {
                case Qt::Key_Down:
                    moveSelection(1);
                    return true;

                case Qt::Key_Up:
                    moveSelection(-1);
                    return true;

                case Qt::Key_Return:
                case Qt::Key_Enter:
                case Qt::Key_Tab:
                    if (acceptCurrent())
                    {
                        return true;
                    }
                    break;

                case Qt::Key_Escape:
                    // seen by nothing else: the page behind the line stays
                    close();
                    return true;

                default:
                    break;
            }
            break;
        }

        case QEvent::FocusOut:
            close();
            break;

        default:
            break;
    }
    return QObject::eventFilter(watched,event);
}

//--------------------------------------------------------------------------

}
