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

/** @file uise/desktop/utils/filenamevalidator.hpp
*
*  Declares FileNameValidator.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_FILENAMEVALIDATOR_HPP
#define UISE_DESKTOP_FILENAMEVALIDATOR_HPP

#include <QString>
#include <QValidator>

#include <uise/desktop/uisedesktop.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

/**
 * @brief Validator of a single file name (basename with extension) that is safe on any file system.
 *
 * Uses the strictest rules of Windows and POSIX combined, because an attachment sent from one
 * platform is saved on another.
 *
 * - Invalid (rejected while typing/pasting): path separators and Windows-reserved characters
 *   (\ / : * ? " < > |), control characters, names longer than 255 UTF-8 bytes.
 * - Intermediate (may be typed, but is not acceptable): empty or whitespace-only name, leading or
 *   trailing whitespace, leading dot (also "." and ".."), trailing dot, two consecutive dots (dots
 *   must be separated by at least one other symbol), Windows reserved device names (CON, PRN, AUX,
 *   NUL, COM0-9, LPT0-9) as the part before the first dot.
 */
class UISE_DESKTOP_EXPORT FileNameValidator : public QValidator
{
    public:

        enum class Problem : int
        {
            None,
            ForbiddenChar,
            TooLong,
            Empty,
            EdgeWhitespace,
            LeadingDot,
            TrailingDot,
            ConsecutiveDots,
            ReservedName
        };

        using QValidator::QValidator;

        State validate(QString& input, int& pos) const override;

        /**
         * @brief Find the first problem of a file name.
         * @param name File name to check.
         * @return Problem::None if the name is valid.
         */
        static Problem check(const QString& name);

        static bool isValid(const QString& name)
        {
            return check(name)==Problem::None;
        }

        /**
         * @brief Short user-facing (translated) description of a problem.
         * @return Empty string for Problem::None.
         */
        static QString problemText(Problem problem);
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_FILENAMEVALIDATOR_HPP
