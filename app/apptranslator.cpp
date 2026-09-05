// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later

#include "apptranslator.h"

#include <QCoreApplication>
#include <QLocale>
#include <QTranslator>

namespace clima {

void AppTranslator::install(QCoreApplication *app)
{
    if (app == nullptr)
        return;

    auto *translator = new QTranslator(app);

    // ":/i18n" is where qt_add_translations puts the compiled catalogues, and
    // "clima_" is the prefix scripts/i18n.sh names them with. QTranslator
    // walks the locale's own fallback chain for us — pt_BR then pt, fr_CA then
    // fr — so a build carrying `clima_fr.qm` serves a Quebec desktop without
    // anybody listing fr_CA anywhere.
    if (!translator->load(QLocale(), QStringLiteral("clima"), QStringLiteral("_"),
                          QStringLiteral(":/i18n"))) {
        // No catalogue for this reader: the source strings are English and
        // they are what they get. Not a failure — see the header — so nothing
        // is logged, and the translator is deleted rather than left installed
        // and empty.
        delete translator;
        return;
    }

    QCoreApplication::installTranslator(translator);
}

} // namespace clima
