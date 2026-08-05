// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Every warning QML emitted, as a thing a test can assert about.
//
// This exists because of a bug that had already shipped once. Launching the app
// printed around 469 `TypeError` and `undefined` lines — bindings evaluating
// before the first snapshot arrived — and every one of them was a card drawing
// with a value it had not been given. The app *looked* right, all the tests
// passed, and the only evidence was a wall of text on a stream nobody reads
// during a build.
//
// A QML warning is not a diagnostic in this codebase; it is a failure that has
// not been noticed yet. `Theme.ink.typo` resolves to `undefined`, which QML
// turns into a transparent colour rather than an error, so the component
// renders invisibly and correctly reports success. The only way to catch that
// class of defect is to treat stderr as an assertion, which is what this is
// for.
//
// Installed for the life of the test binary. `clear()` before the interesting
// work and `count` after it is the pattern; `messages` is there so a failure
// says which warning rather than how many.
#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QStringList>

class QmlWarnings : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(int count READ count NOTIFY changed)
    Q_PROPERTY(QStringList messages READ messages NOTIFY changed)

public:
    explicit QmlWarnings(QObject *parent = nullptr);
    ~QmlWarnings() override;

    int         count() const;
    QStringList messages() const;

    // Called from main() before the test engine exists, so that a warning
    // emitted while a test file is being *loaded* is recorded too.
    static void install();

    Q_INVOKABLE void clear();

    // The one-line summary a failure message wants: the first few warnings,
    // rather than all of them, because a broken binding usually emits one per
    // frame and a test log with four hundred identical lines in it is a test
    // log nobody reads either.
    Q_INVOKABLE QString summary() const;

Q_SIGNALS:
    void changed();
};
