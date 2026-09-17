/**
@copyright Evgeny Sidorov 2022

This software is dual-licensed. Choose the appropriate license for your project.

1. The GNU GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-GPLv3.md](LICENSE-GPLv3.md) or copy at https://www.gnu.org/licenses/gpl-3.0.txt)
    
2. The GNU LESSER GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-LGPLv3.md](LICENSE-LGPLv3.md) or copy at https://www.gnu.org/licenses/lgpl-3.0.txt).

You may select, at your option, one of the above-listed licenses.

*/

/****************************************************************************/

/** @file uise/desktop/src/editablelabel.cpp
*
*  Defines EditableLabel.
*
*/

/****************************************************************************/

#include <QKeyEvent>
#include <QMenu>
#include <QClipboard>
#include <QGuiApplication>
#include <QSizePolicy>
#include <QTimer>
#include <QtMath>
#include <iostream>   // ELBL-DEBUG temporary

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/style.hpp>
#include <uise/desktop/editablepanel.hpp>
#include <uise/desktop/label.hpp>
#include <uise/desktop/editablelabel.hpp>

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot expand a macro-opened
// namespace, so it records tr() calls in this file under an unqualified context that does not
// match what moc (a real preprocessor) resolves at runtime -- translations for every string here
// would silently stay in English. Do not revert to the macro form. See task-localization-framework.md.
namespace uise {

namespace {

/**
 * @brief Value label used by EditableLabel in non-editing mode, floored at one full text line.
 *
 * QLabel asks the layout for one height (QLabelPrivate::sizeForWidth() = document size +
 * contentsMargins) but paints inside another: paintEvent() clips to layoutRect(), whose height IS
 * contentsRect().height(), and clamps the vertical-centering offset to >= 0 when the text overflows
 * it -- so any shortfall is cut off the *bottom*, descenders first ("yyyy" renders as "vvvv"), while
 * the QSS background/border keep painting past the clip.
 *
 * Those two numbers can disagree for a QSS-styled label: the QSS box (padding, and under whitem's
 * theme a bottom border) becomes contentsMargins only once QFrame applies it (on Polish/StyleChange,
 * i.e. after the label has already been measured once), and it is not accounted for at all by
 * QStyleSheetStyle::sizeFromContents (there is no CT_Label case). Under whitem's theme (padding 6px
 * + border-bottom 2px, 14px font) the label ends up ~3px short of one line.
 *
 * Rather than chase that gap through Qt's style/polish event sequence -- which has real holes, e.g.
 * Style::updateWidgetStyle()'s unpolish()+polish() repolish path (used for per-widget dynamic
 * property changes) applies a new QSS box without sending StyleChange/ContentsRectChange/FontChange
 * at all -- this floors the label's own size hints directly, so every layout pass self-corrects
 * regardless of *how* the box or font last changed. It is a floor only: a wrapped value that needs
 * more still reports its own larger height, and a hidden label (editing mode swaps it out) is empty
 * for the layout, so nothing here affects the editor.
 */
class ValueLabel : public Label
{
    public:

        using Label::Label;

        int heightForWidth(int w) const override
        {
            return qMax(Label::heightForWidth(w),minHeightForWidth(w));
        }

        QSize sizeHint() const override
        {
            auto sz=Label::sizeHint();
            sz.setHeight(qMax(sz.height(),minHeightForWidth(sz.width())));
            return sz;
        }

        QSize minimumSizeHint() const override
        {
            auto sz=Label::minimumSizeHint();
            sz.setHeight(qMax(sz.height(),minHeightForWidth(sz.width())));
            return sz;
        }

    private:

        int minHeightForWidth(int width) const
        {
            const auto m=contentsMargins();

            // Not QFontMetrics::height(): it rounds ascent and descent independently and drops
            // the leading (qfontmetrics.cpp), so it can land a pixel BELOW the line height the
            // text layout itself uses -- qCeil(ascent + descent + max(0, leading))
            // (qtextlayout.cpp + qtextengine_p.h, with QTextDocumentLayout's leadingIncluded=true).
            // Ceil the sum from QFontMetricsF instead so the floor can never itself run short.
            const QFontMetricsF fm(font(),this);
            int need=qCeil(fm.ascent()+fm.descent()+qMax(qreal(0),fm.leading()));

            // Wrapped text: same measurement QLabel itself falls back to for a plain-text label
            // with no text control (QLabelPrivate::sizeForWidth), so a value that wraps to N lines
            // is floored at N full lines rather than just one.
            const int textWidth=width-m.left()-m.right();
            if (wordWrap() && textWidth>0 && !text().isEmpty())
            {
                const int flags=Qt::TextWordWrap
                    |(alignment() & ~(Qt::AlignVCenter|Qt::AlignHCenter));
                need=qMax(
                    need,
                    QFontMetrics(font(),const_cast<ValueLabel*>(this))
                        .boundingRect(0,0,textWidth,QWIDGETSIZE_MAX,flags,text()).height()
                );
            }

            return m.top()+m.bottom()+need;
        }
};

/**
 * @brief Forces every ancestor layout of @b widget to recompute, bypassing the visibility gate
 * that silently swallows a plain QWidget::updateGeometry() call.
 *
 * QWidgetPrivate::updateGeometry_helper() -- what a changed sizeHint()/sizePolicy() normally goes
 * through -- is a documented no-op while the widget "isHidden()": it skips invalidating the parent
 * layout entirely (qwidget.cpp). EditableLabel's rows are built, and their QSS box settles via
 * Polish, before the page that hosts them is ever shown, so that call is swallowed every time.
 * Worse, it stays swallowed: QLayout::activate() returns immediately once its own `activated` flag
 * is set, and nothing ever clears that flag if invalidate() was never reached -- so even once the
 * page is later shown (which only calls activate(), see QWidgetPrivate::show_recursive()), the row
 * is stuck forever at whatever size its very first, premature activation computed. This is why the
 * label's sizeHint() can report the correct floored height while its actual height() stays frozen
 * at the old, too-short value from before ValueLabel's fix (or from before the QSS box even
 * applied) -- the corrected hint never reaches the layout that owns the widget's real geometry.
 *
 * QLayout::invalidate() has no such visibility check, so calling it directly up the whole ancestor
 * chain -- not just the nearest layout, since every level above it may be equally stuck -- clears
 * every stale `activated` flag in one pass. The next activation (on show, or immediately if already
 * visible) then recomputes from the widgets' current, correct size hints.
 */
void invalidateAncestorLayouts(QWidget* widget)
{
    for (auto w=widget; w!=nullptr; w=w->parentWidget())
    {
        if (auto l=w->layout())
        {
            l->invalidate();
        }
    }
}

// ELBL-DEBUG temporary -- brute-force version of invalidateAncestorLayouts() used only to test,
// causally and live, whether "every stale QLayout::activated flag" really is the whole story:
// invalidates EVERY layout in the entire subtree (not just the ancestor chain), then forces the
// top-level window's own layout to activate and repaint. If triggering this from the UI (see the
// "ELBL-DEBUG: Force relayout" context-menu action below) makes the clipping visibly disappear,
// the ancestor-only theory was right but incompletely applied; if it does NOT, the theory is wrong
// and the cap is coming from somewhere else entirely. Remove this whole function, its call site,
// and the temporary <iostream> include once the question is settled.
void debugForceFullRelayout(QWidget* w)
{
    if (auto l=w->layout())
    {
        l->invalidate();
    }
    const auto children=w->findChildren<QWidget*>(QString(),Qt::FindDirectChildrenOnly);
    for (auto c : children)
    {
        debugForceFullRelayout(c);
    }
}

}

//--------------------------------------------------------------------------

EditableLabel::EditableLabel(
        Type type,
        QWidget* parent,
        bool inGroup
    ) : AbstractValueWidget(parent),
        m_type(type),
        m_label(new ValueLabel(this)),
        m_formatter(nullptr),
        m_editing(false),
        m_inGroup(inGroup),
        m_panel(nullptr),
        m_editable(true),
        m_editButtonAlwaysHidden(false),
        m_trailingWidget(nullptr)
{
    m_mainLayout=Layout::vertical(this);

    auto mainFrame=new QFrame(this);
    m_mainLayout->addWidget(mainFrame);

    m_layout=Layout::horizontal(mainFrame);
    m_layout->addWidget(m_label,100);
    m_label->setObjectName("label");
    m_label->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);

    m_label->installEventFilter(this);
    m_label->setWordWrap(true);

    m_editorFrame=new QFrame(this);
    m_editorFrame->setObjectName("labelEditorFrame");
    m_editorLayout=Layout::horizontal(m_editorFrame);
    m_layout->addWidget(m_editorFrame,100);
    m_editorFrame->setVisible(false);

    m_buttonsFrame=new QFrame(this);
    m_buttonsFrame->setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Preferred);
    m_buttonsFrame->setObjectName("labelButtonsFrame");
    m_buttonsLayout=Layout::horizontal(m_buttonsFrame);
    m_layout->addWidget(m_buttonsFrame,Qt::AlignBaseline | Qt::AlignLeft);
    m_buttonsFrame->setVisible(!m_inGroup);

    m_editButton = new PushButton(Style::instance().svgIconLocator().icon("EditableLabel::edit",this),this);
    m_editButton->setProperty("labelButton",true);
    m_editButton->setToolTip(tr("Edit"));
    m_editButton->setObjectName("editButton");
    connect(m_editButton,&PushButton::clicked,this,&EditableLabel::edit);
    m_buttonsLayout->addWidget(m_editButton,Qt::AlignBaseline | Qt::AlignLeft);

    m_applyButton = new PushButton(Style::instance().svgIconLocator().icon("EditableLabel::apply",this),this);
    m_applyButton->setProperty("labelButton",true);
    m_applyButton->setToolTip(tr("Apply"));
    m_applyButton->setObjectName("applyButton");
    connect(m_applyButton,&PushButton::clicked,this,&EditableLabel::apply);
    m_buttonsLayout->addWidget(m_applyButton,Qt::AlignBaseline | Qt::AlignLeft);
    m_applyButton->setVisible(false);

    m_cancelButton = new PushButton(Style::instance().svgIconLocator().icon("EditableLabel::cancel",this),this);
    m_cancelButton->setProperty("labelButton",true);
    m_cancelButton->setToolTip(tr("Cancel"));
    m_cancelButton->setObjectName("cancelButton");
    m_cancelButton->setVisible(false);
    connect(m_cancelButton,&PushButton::clicked,this,&EditableLabel::cancel);
    m_buttonsLayout->addWidget(m_cancelButton,Qt::AlignBaseline | Qt::AlignLeft);

    m_comment=new QLabel(mainFrame);
    m_comment->setObjectName("comment");
    m_comment->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_comment->setWordWrap(true);
    m_mainLayout->addWidget(m_comment);
    m_comment->setVisible(false);

    updateRowMinHeight();
}

//--------------------------------------------------------------------------

void EditableLabel::updateRowMinHeight()
{
    m_rowMinHeightUpdatePending=false;

    // Row-height parity between the two modes: QHBoxLayout ignores a hidden item's size entirely
    // (m_label and m_editorFrame are never both visible at once), so without this the row is
    // exactly as tall as whichever one currently shows, and toggling edit mode visibly nudges it.
    // minimumSizeHint() is QLabel's own "height for one line" figure (independent of the label's
    // current wrap state), which is the right target for a single-line editor to match.
    if (m_editorFrame!=nullptr)
    {
        m_editorFrame->setMinimumHeight(m_label->minimumSizeHint().height());
    }

    // The clipping fix itself lives in ValueLabel (m_label's concrete class): it floors its own
    // size hints correctly. But a correct size HINT is not enough -- the row is very often built
    // (and its QSS box settles via Polish) before the page that hosts it is ever shown, and
    // QWidget::updateGeometry() silently no-ops while hidden, which leaves every ancestor layout's
    // "activated" flag stuck true forever (see invalidateAncestorLayouts()'s doc comment). Force
    // it directly on every call here, not just once: this same call also runs right after
    // setEditing() toggles which widget occupies the row, so a freshly-shown editor/label gets its
    // real geometry too, not just whatever its ancestor layout computed the first time around.
    invalidateAncestorLayouts(this);
}

//--------------------------------------------------------------------------

void EditableLabel::scheduleRowMinHeightUpdate()
{
    if (m_rowMinHeightUpdatePending)
    {
        return;
    }
    m_rowMinHeightUpdatePending=true;
    QTimer::singleShot(0,this,[this](){updateRowMinHeight();});
}

//--------------------------------------------------------------------------

EditableLabel::EditableLabel(Type type, AbstractEditablePanel* panel)
    : EditableLabel(type,panel,true)
{
    setEditablePanel(panel);
}

//--------------------------------------------------------------------------

void EditableLabel::setTrailingWidget(QWidget* widget)
{
    if (m_trailingWidget==widget)
    {
        return;
    }

    if (m_trailingWidget!=nullptr)
    {
        m_layout->removeWidget(m_trailingWidget);
        m_trailingWidget->deleteLater();
    }

    m_trailingWidget=widget;
    if (m_trailingWidget!=nullptr)
    {
        // Appended, so it lands after m_buttonsFrame - the trailing edge of the row. Stretch 0:
        // the label (and the editor that replaces it) keep all the slack, exactly as they do
        // against the buttons frame. addWidget() reparents into m_layout's own widget (the inner
        // mainFrame, not this), which is where every other row element lives too.
        m_layout->addWidget(m_trailingWidget,0);
    }
}

//--------------------------------------------------------------------------

bool EditableLabel::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::KeyPress && watched == editor())
    {
        QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key()==Qt::Key_Escape)
        {
            if (m_inGroup)
            {
                emit groupCancelRequested();
            }
            else
            {
                cancel();
            }
        }
        if (keyEvent->key()==Qt::Key_Return && !m_inGroup)
        {
            if (editor()->metaObject()->metaType()!=QTextEdit::staticMetaObject.metaType())
            {
                apply();
            }
        }
    }
    else if (watched == m_label)
    {
        if (event->type() == QEvent::MouseButtonDblClick)
        {
            if (m_editable)
            {
                if (m_inGroup)
                {
                    if (isGroupEditingRequestEnabled())
                    {
                        emit groupEditingRequested();
                        QTimer::singleShot(
                            10,
                            this,
                            [this]()
                            {
                                editor()->setFocus();
                            }
                        );
                        return true;
                    }
                    return false;
                }

                setEditing(true);
                return true;
            }
            return false;
        }
        else if (event->type() == QEvent::ContextMenu)
        {
            auto menu=new QMenu(m_label);

            if (!m_inGroup)
            {
                auto edit = menu->addAction(tr("Edit"),this,[this](){setEditing(true);});
                menu->setDefaultAction(edit);
            }

            menu->addAction(tr("Copy"),this,[this](){
                auto selected = m_label->selectedText();
                if (selected.isEmpty())
                {
                    selected = m_label->text();
                }
                QGuiApplication::clipboard()->setText(selected);
            });

            // ELBL-DEBUG temporary -- see debugForceFullRelayout()'s doc comment.
            menu->addAction(QStringLiteral("ELBL-DEBUG: Force relayout"),this,[this](){
                auto win=m_label->window();
                std::cerr << "ELBL-DEBUG before: m_label height=" << m_label->height()
                           << " editor height=" << (editor() ? editor()->height() : -1)
                           << std::endl;
                debugForceFullRelayout(win);
                if (auto wl=win->layout())
                {
                    wl->activate();
                }
                win->updateGeometry();
                win->update();
                std::cerr << "ELBL-DEBUG after: m_label height=" << m_label->height()
                           << " editor height=" << (editor() ? editor()->height() : -1)
                           << std::endl;
            });

            menu->exec(QCursor::pos());
            return true;
        }
        else if (event->type()==QEvent::ContentsRectChange
                 || event->type()==QEvent::StyleChange
                 || event->type()==QEvent::FontChange)
        {
            // Recompute the row's geometry once the box (QSS padding/border) or font that
            // determines it has actually changed -- e.g. a light/dark theme swap, which fires all
            // three of these together. Deferred: an event filter runs before the watched widget's
            // own event()/changeEvent(), and on StyleChange it's QFrame::changeEvent() that applies
            // the new QSS box to contentsMargins(), so reading it here directly would see the stale
            // value. Falls through to return false so m_label still gets to handle the event itself.
            scheduleRowMinHeightUpdate();
        }
    }

    return false;
}

//--------------------------------------------------------------------------

void EditableLabel::setEditablePanel(AbstractEditablePanel* panel)
{
    setInGroup(config().property(ValueWidgetProperty::InGroup,true).toBool());
    m_panel=panel;
    updateControls();
    if (m_inGroup)
    {
        connect(
            panel,
            &AbstractEditablePanel::editRequested,
            this,
            &EditableLabel::edit
        );
        connect(
            panel,
            &AbstractEditablePanel::cancelRequested,
            this,
            &EditableLabel::cancel
        );
        connect(
            panel,
            &AbstractEditablePanel::applyCommited,
            this,
            &EditableLabel::apply
        );
        connect(
            this,
            &AbstractValueWidget::valueEdited,
            panel,
            &AbstractEditablePanel::contentEdited
        );
    }
}

//--------------------------------------------------------------------------

void EditableLabel::setComment(const QString& comment)
{
    m_comment->setText(comment);
    m_comment->setVisible(!comment.isEmpty());
}

QString EditableLabel::comment() const
{
    return m_comment->text();
}

//--------------------------------------------------------------------------

}
