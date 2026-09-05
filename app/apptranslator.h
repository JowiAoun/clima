// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The reader's language, if this build carries it.
//
// ============================================================================
// THERE ARE NO TRANSLATIONS YET, AND THIS IS STILL WORTH HAVING
//
// `app/translations/clima.ts` is the source catalogue — 271 strings, kept
// current by scripts/i18n.sh and gated in CI — and there is not one language
// catalogue beside it. docs/known-gaps.md says so rather than shipping a
// machine translation, which is the one thing worse than shipping none: a
// reader who sees their own language done badly concludes the app is careless
// about everything else too.
//
// What this class is, then, is the half that turns a translation into a
// running program. Drop `clima_fr.ts` into app/translations, add its line to
// app/CMakeLists.txt, and French appears — no code changes, because the lookup
// below is by locale and not by a list of languages somebody has to remember
// to extend.
//
// ============================================================================
// WHY IT ASKS QLocale AND NOT Settings
//
// There is no language preference and there should not be one until there are
// languages. QLocale() is the desktop's own answer, which is where every other
// application on the machine gets it, and an app that made its reader choose
// twice would be the odd one out. If a preference is ever wanted it belongs
// beside the clock format in Settings, and this function grows one argument.
//
// ============================================================================
// THE MISSING CATALOGUE IS THE ORDINARY CASE
//
// `QTranslator::load()` answers false when there is nothing to load, which is
// every build today and every build for a locale nobody has translated. That
// is not an error and it is not reported as one: the source strings are
// English and they are what the reader gets. Nothing is installed, so nothing
// costs anything at run time.

#pragma once

class QCoreApplication;

namespace clima {

class AppTranslator
{
public:
    // Installs a translator for the system locale when a catalogue for it was
    // compiled into this binary. Call once, from main(), before the QML engine
    // loads — a translation installed after a component is built does not
    // reach the strings already in it.
    //
    // `app` owns whatever is installed.
    static void install(QCoreApplication *app);
};

} // namespace clima
