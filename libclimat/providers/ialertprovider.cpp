// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: MPL-2.0
//
// The out-of-line destructor, and nothing else. It exists so that the class has
// a translation unit to anchor its vtable in — the same reason
// iforecastprovider.cpp exists, and the same reason a header-only interface
// emits a copy of the vtable into every object file that includes it.

#include "libclimat/providers/ialertprovider.h"

namespace climat {

IAlertProvider::~IAlertProvider() = default;

} // namespace climat
