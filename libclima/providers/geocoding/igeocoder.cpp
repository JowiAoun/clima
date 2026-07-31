// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: MPL-2.0
//
// Out-of-line destructors. Both interfaces are pure virtual and neither has a
// single non-inline member otherwise, which is exactly the case where a
// compiler emits the vtable into every translation unit that includes the
// header. One key function pins it to one object file.

#include "igeocoder.h"

namespace clima {

IForwardGeocoder::~IForwardGeocoder() = default;
IReverseGeocoder::~IReverseGeocoder() = default;

} // namespace clima
