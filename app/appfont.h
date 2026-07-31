// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The application font.
//
// Until this file existed, `grep -rn 'font.family\|FontLoader' app/qml` returned
// nothing at all, next to 162 lines that set `font.pixelSize`. That is not "the
// default font", it is *a different font per machine*: fontconfig picks one on
// Linux out of whatever the distro installed, DirectWrite picks one on Windows,
// CoreText picks one on macOS. On the machine this was written on the answer was
// DejaVu Sans. Two consequences, and the second is the expensive one.
//
//   The product looks different on every operating system. Not subtly — the
//   metrics differ, so the wrap points differ, so a two-line body that fits in a
//   fixed-height card here overflows it there, and nobody sees that until a user
//   posts a screenshot.
//
//   Golden images cannot be compared across machines. A pixel test is a test
//   only if the same input gives the same pixels somewhere other than the
//   machine that recorded them, and text is most of these pixels. Without this
//   the whole of W2's image comparison is a coin toss between runners.
//
// So the app carries its own faces and stops asking. See install().
//
// ---- why this is in the library and not in a main() -------------------------
//
// There are two executables. The gallery exists to review the components the app
// draws, and a gallery drawing them in a different typeface reviews something
// the product does not ship. Both mains call install(); the font files are in
// the binary once, because both link `climaqml`, which is where the resource
// lives.

#ifndef CLIMA_APPFONT_H
#define CLIMA_APPFONT_H

#include <QString>

namespace AppFont {

// Registers the bundled faces and makes them the application font.
//
// Call once, after the QGuiApplication exists — the font database needs the
// platform integration up — and before the QML engine loads anything, because a
// Text item resolves its family from the application font when it is created.
//
// Returns the family name that was installed, which is the name the *files*
// declare rather than a string typed in here, and an empty string if nothing
// could be registered. Failure warns and is not fatal: rendering in the host's
// font is worse than rendering in ours, and far better than not starting.
QString install();

} // namespace AppFont

#endif // CLIMA_APPFONT_H
