// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <string>

/**
 * Creates a per-application configuration file derived from a base config.
 * VSOMEIP_CONFIGURATION is used as source path.
 * If VSOMEIP_CONFIGURATION is empty/unset, the function fails.
 *
 * The configuration is copied and modified so that `network`, `name`, and `routing` are replaced with `_app_name`.
 *
 * The function exports `VSOMEIP_CONFIGURATION_<_app_name>` pointing to the generated file.
 *
 * @return 0 on success and 1 on failure.
 */
bool create_config(std::string _app_name);
