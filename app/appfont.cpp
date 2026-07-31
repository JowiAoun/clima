// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later

#include "appfont.h"

#include <QFontDatabase>
#include <QGuiApplication>
#include <QStringList>

#include <array>

namespace {

// ---- why two static faces and not one variable file -------------------------
//
// Inter ships a variable font, InterVariable.ttf, whose `wght` axis runs 100 to
// 900 and which declares nine named instances including Bold. It is one file
// instead of two, it is the format the project recommends, and it is the wrong
// choice here. Measured on this Qt — 6.11.1, both the offscreen and the xcb
// platform plugins, same answer from each:
//
//     QFontDatabase::applicationFontFamilies()  ->  ("Inter Variable")
//     QFontDatabase::styles("Inter Variable")   ->  ("Regular")
//
// One style. Qt's FreeType font database registers the file's default instance
// and does not expand the other eight, so `font.bold: true` — which 58 lines in
// the QML tree set — does not select Inter Bold. It selects Inter Regular and
// asks FreeType to fatten the outline, and the giveaway is the advance width:
//
//     "Heavy rain expected 27° Weather details" at 34 px
//       variable, regular      654.875
//       variable, font.bold    654.875   <- identical: no glyph changed width
//       variable, wght=700     674.656
//       static Inter-Bold      674.297
//
// Synthetic bold is real Bold's strokes squeezed into Regular's spacing. It
// reads as heavy and slightly smeared, and it wraps at different points than
// the face the designer drew.
//
// The deeper objection is that it reintroduces the defect this file exists to
// remove. Whether a variable font's named instances become selectable styles is
// a property of the *font database*, not of the font: Qt's FreeType backend does
// not expand them, DirectWrite and CoreText enumerate instances natively, and
// which of those a build gets depends on the host. A bold heading that is
// synthesised on Linux and drawn from the real Bold outlines on Windows is
// exactly "renders differently on every operating system", arrived at by a
// different road.
//
// Two static faces have no such ambiguity. Regular and Bold are two ordinary
// TrueType files, every font database on every platform registers them the same
// way, and `font.bold` picks the second one everywhere. They also cost less:
// 832 KB for the pair against 880 KB for the variable file.
//
// Reach for the variable font when a weight between these two is wanted — a
// Medium for a settings surface, say. `QFont::setVariableAxis` addresses the
// axis directly and does not depend on instance enumeration, so it works today;
// it just cannot be reached through `font.bold`.
//
// ---- the paths --------------------------------------------------------------
//
// `:/qt/qml/Clima/fonts/…`, which is where qt_add_qml_module's RESOURCES put a
// file listed as `fonts/Inter-Regular.ttf`: the module's own resource prefix,
// then the path relative to app/CMakeLists.txt. Not aliased flat the way the
// .qml and .js files are — those are flattened so that the module directory
// matches its URI, and a font is not a QML type, so a subdirectory is just a
// subdirectory.
constexpr auto kFontResources = std::array{
    ":/qt/qml/Clima/fonts/Inter-Regular.ttf",
    ":/qt/qml/Clima/fonts/Inter-Bold.ttf",
};

} // namespace

QString AppFont::install()
{
    // Collected rather than taken from the first file, because both files
    // declare the same family — "Inter", styles Regular and Bold — and the
    // check worth making is that they *agree*. Two faces registering two
    // families is a packaging mistake (someone dropped in InterDisplay-Bold,
    // whose family is "Inter Display") and its symptom is a bold heading in a
    // face that does not match the body around it. Silent, and easy to look at
    // for a week without seeing.
    QStringList families;
    for (const auto *path : kFontResources) {
        const int id = QFontDatabase::addApplicationFont(QString::fromLatin1(path));
        if (id < 0) {
            qWarning("clima: could not register %s; text will render in the host's font.", path);
            continue;
        }
        for (const QString &family : QFontDatabase::applicationFontFamilies(id)) {
            if (!families.contains(family))
                families.append(family);
        }
    }

    if (families.isEmpty()) {
        qWarning("clima: no bundled font could be registered; text will render in the host's font.");
        return {};
    }

    if (families.size() > 1) {
        qWarning("clima: the bundled faces declare %lld families (%s); using the first. "
                 "They are meant to be one family in two weights.",
                 static_cast<long long>(families.size()), qPrintable(families.join(u", ")));
    }

    const QString family = families.constFirst();

    // Modify the application font rather than construct one. A QFont built from
    // a family name alone leaves every other field unresolved, and assigning it
    // back is how an app quietly loses the point size the platform chose for it
    // — which is the size the user's desktop asked for, at the DPI they asked
    // for, and none of our business.
    //
    // Nothing visible depends on that size today: all 158 Text items in the two
    // QML modules set `font.pixelSize` from a Theme token, the one that does not
    // copies another's whole font. It matters the first time one does not, and
    // it is the reason this is two lines rather than one.
    QFont font = QGuiApplication::font();
    font.setFamily(family);
    QGuiApplication::setFont(font);

    return family;
}
