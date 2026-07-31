// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: MPL-2.0

#include "validatorstore.h"

namespace clima {

// Out of line so the vtable has a home translation unit rather than being
// emitted into every file that includes the header.
ValidatorStore::~ValidatorStore() = default;

} // namespace clima
