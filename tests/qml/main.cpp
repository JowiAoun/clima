// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The QML test runner.
//
// Everything under tests/ before this file tested libclima — a network client,
// a cache, an adapter — and one view model. That is about 3,500 lines of C++.
// The other 13,400 lines of this application are QML, and none of it had a test
// of any kind: the acceptance check for the whole port was that `--grab`
// produced identical bytes, which proves a scene renders the same as it used to
// and says nothing about whether it renders the same as it *should*.
//
// QtQuickTest rather than a hand-rolled harness because it brings the one thing
// that is tedious to build: a QQmlEngine per test file, with the module import
// path already set up, and per-function reporting that ctest can read.
//
// The tests are QML because their subject is QML. A C++ test that reaches into
// a QQmlEngine to instantiate `DetailUvCard` and read a property back is
// testing the same thing through a keyhole, and it cannot express the assertion
// that matters most here — that the component's *bindings* resolve.
#include <QtQuickTest/quicktest.h>

#include <QGuiApplication>
#include <QQmlEngine>

#include "appengine.h"
#include "appfont.h"
#include "libclima/providers/fixture/fixtureprovider.h"
#include "support/networkguard.h"
#include "qmlwarnings.h"
#include "settings.h"

// Both modules, and the second one is not a convenience. `Clima.Gallery` holds
// gallery.js — the catalogue of every component in the tree, which is what
// tst_specimen walks — and contrast.js, the WCAG arithmetic the palette page
// audits with. Testing against the same catalogue the gallery browses is the
// point: a component added to the tree and forgotten in the catalogue is
// already visible as a gap in the gallery, and now it is the same gap in CI.
class Setup : public QObject
{
    Q_OBJECT

public:
    Setup() = default;

public Q_SLOTS:
    void applicationAvailable()
    {
        // Everything below mirrors app/main.cpp, in its order, and that is the
        // requirement rather than a convenience.
        //
        // The first honest run of tst_specimen failed on three `TypeError:
        // Cannot read property 'count' of null` in PlacePicker, because
        // `Engine.places` and `Engine.search` are built by AppEngine::configure
        // and nothing here had called it. The app calls it in main() before the
        // QML engine loads, so those models are never null in the real
        // program — which means a test that leaves them null is not finding a
        // bug, it is inventing a state and then reporting it.
        //
        // Testing components against an engine the application never runs is
        // worse than useless: it is a suite that goes red for reasons the
        // product cannot have, which is how a suite gets ignored.

        // Format decided at construction and never revisited; before anything
        // builds a QSettings. XDG_CONFIG_HOME and XDG_DATA_HOME are redirected
        // into the build tree by tests/qml/CMakeLists.txt, so a test run does
        // not write to the developer's own configuration.
        Settings::prepareStorage();

        // The bundled face, installed exactly as the app installs it. Type
        // metrics decide wrap points, a wrap point decides an implicit height,
        // so a test asserting a card's size would otherwise be asserting
        // something about the machine's fontconfig.
        AppFont::install();

        // Nothing here may open a socket. The fixture provider is offline and
        // no test types into a search box, so today nothing tries — this makes
        // that a guarantee rather than an observation, and the same guarantee
        // tst_networkisolation holds the engine to.
        NetworkGuard::install();

        // A frozen clock and a recorded forecast, which is what `--grab` and
        // the whole of CI already run on. Components get real values to bind
        // to rather than empty models, so a good deal more of each one is
        // actually exercised by being built.
        AppEngine::instance()->configure(clima::fixtures::defaultName());

        // Installed after QGuiApplication exists but before any test file is
        // loaded, so a warning raised while a .qml is being parsed is caught
        // rather than merely printed. Re-asserted on every clear(); see
        // qmlwarnings.cpp for the reason that is not paranoia.
        QmlWarnings::install();
    }
};

QUICK_TEST_MAIN_WITH_SETUP(clima_qml, Setup)

#include "main.moc"
