// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later

#include "widgetclock.h"

namespace clima::widgets {
namespace {

// File-scope rather than a class, because there is one process and one clock in
// it. A settable singleton object would be the same state with a constructor in
// front of it.
QDateTime g_frozen;

} // namespace

QDateTime now()
{
    return g_frozen.isValid() ? g_frozen : QDateTime::currentDateTimeUtc();
}

void freezeClock(const QDateTime &instant)
{
    g_frozen = instant.isValid() ? instant.toUTC() : QDateTime();
}

bool clockIsFrozen()
{
    return g_frozen.isValid();
}

} // namespace clima::widgets
