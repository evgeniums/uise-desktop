/**
@copyright Evgeny Sidorov 2022-2025

This software is dual-licensed. Choose the appropriate license for your project.

1. The GNU GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-GPLv3.md](LICENSE-GPLv3.md) or copy at https://www.gnu.org/licenses/gpl-3.0.txt)

2. The GNU LESSER GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-LGPLv3.md](LICENSE-LGPLv3.md) or copy at https://www.gnu.org/licenses/lgpl-3.0.txt).

You may select, at your option, one of the above-listed licenses.

*/

/****************************************************************************/

/** @file uise/desktop/chataudiofileitems.hpp
*
*  Declares ChatVoiceFileItem and ChatAudioFileItem, the chat file rows for voice messages and audio files.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_CHATAUDIOFILEITEMS_HPP
#define UISE_DESKTOP_CHATAUDIOFILEITEMS_HPP

#include <memory>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/chatmessagefileitem.hpp>
#include <uise/desktop/waveformbar.hpp>

class QShowEvent;

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot expand a macro-opened
// namespace, so it records tr() calls in this file under an unqualified context that does not
// match what moc (a real preprocessor) resolves at runtime -- translations for every string here
// would silently stay in English. Do not revert to the macro form. See task-localization-framework.md.
namespace uise {

class ChatVoiceFileItem_p;

/**
 * @brief The row of a voice message: a play button, the "Voice message" title, a seekable
 *  waveform, and the duration and size with a "listened by peer" dot before them.
 *
 * A ChatMessageFileItem, so it is what a ChatFileItemBuilder returns for an item that
 * ChatFileItem::isVoice(); makeChatAudioFileItem() does that choice. Everything it shows comes from
 * the ChatFileItem -- the waveform, the duration, the listened flag, whether it is playing -- so it
 * survives the row being rebuilt. Only a playing message moves without a refresh: its waveform
 * through setPlaybackProgress(), and its info line's clock (position replacing "duration · size"
 * while it plays) through setPlaybackPosition().
 *
 * The play button sits over the icon slot and is shown only while the file is available; while it
 * is downloading the slot shows the usual load control, and the waveform, which came with the
 * message, is drawn but cannot be sought.
 *
 * It is backend-free: it only sends playRequested(), stopRequested() and seekRequested(). The
 * button toggles by ChatFileItem::isPlaying(), so a host that treats "stop" of a voice message as
 * pause gets a play/pause button.
 */
class UISE_DESKTOP_EXPORT ChatVoiceFileItem : public ChatMessageFileItem
{
    Q_OBJECT

    public:

        explicit ChatVoiceFileItem(QWidget* parent=nullptr);
        ~ChatVoiceFileItem();

        ChatVoiceFileItem(const ChatVoiceFileItem&)=delete;
        ChatVoiceFileItem(ChatVoiceFileItem&&)=delete;
        ChatVoiceFileItem& operator=(const ChatVoiceFileItem&)=delete;
        ChatVoiceFileItem& operator=(ChatVoiceFileItem&&)=delete;

        void refresh() override;

        //! Move the waveform's progress. Ignored while the user is dragging it.
        void setPlaybackProgress(qreal fraction) override;

        //! Move the info line's clock. See AbstractChatMessageFiles::setPlaybackPosition().
        void setPlaybackPosition(qint64 positionMs, qint64 durationMs) override;

        WaveformBar* waveformBar() const noexcept;

    protected:

        //! Takes the click on the play tile.
        bool eventFilter(QObject* watched, QEvent* event) override;

        void changeEvent(QEvent* event) override;

    private:

        void updatePlayButton();
        void retranslate();

        //! The single writer of the info line: "position / duration" while playing (pimpl's
        //! showingPosition), else "duration · size" -- refresh()'s idle case and
        //! setPlaybackPosition()'s playing case both funnel through here.
        void updateInfoText();

        //! Reserves infoLabel()'s minimum width from the widest-digits mask of \p longest, so a
        //! proportional font's changing digits (e.g. "0:01" vs "0:02") never resize the row on
        //! every tick -- same technique as AudioPlayerWidget::updateTimeLabelWidth().
        void reserveInfoWidth(const QString& longest);

        std::unique_ptr<ChatVoiceFileItem_p> pimpl;
};

/**
 * @brief The row of an ordinary audio file: the usual file row with a play glyph as its icon.
 *
 * The click on the icon is the base row's clicked(), which a host maps to "play" for an audio
 * item, as it maps it to "open" for any other; the menu's Play and Stop are the other way in.
 * While the item is playing the glyph is a pause.
 */
class UISE_DESKTOP_EXPORT ChatAudioFileItem : public ChatMessageFileItem
{
    Q_OBJECT

    public:

        using ChatMessageFileItem::ChatMessageFileItem;

        void refresh() override;

    protected:

        //! The icon can only be replaced while the icon slot is visible, which it is not yet on
        //! the first refresh() of a row that is built before it is shown.
        void showEvent(QShowEvent* event) override;

    private:

        void applyIcon();
};

/**
 * @brief A ChatFileItemBuilder for audio: a ChatVoiceFileItem for a voice message, a
 *  ChatAudioFileItem for any other audio file, nullptr (the default row) for everything else.
 *
 * A host that needs other kinds too calls this from its own builder for the kinds it does not
 * handle first, the way an invitation row is chosen.
 */
UISE_DESKTOP_EXPORT ChatMessageFileItem* makeChatAudioFileItem(const ChatFileItem& item, QWidget* parent);

}

#endif // UISE_DESKTOP_CHATAUDIOFILEITEMS_HPP
