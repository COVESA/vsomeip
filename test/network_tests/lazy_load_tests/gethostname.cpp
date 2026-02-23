// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <unistd.h>
#include <cstdlib>
#include <cstring>

// Intercept gethostname calls to inject one defined in VSOMEIP_HOSTNAME env
int gethostname(char* name, size_t size) {
    if (size == 0)
        return -1;

    const char* env_hostname = std::getenv("VSOMEIP_HOSTNAME");
    if (env_hostname != nullptr && env_hostname[0] != '\0') {
        strncpy(name, env_hostname, size - 1);
        // in case of env_hostname being bigger than size
        name[size - 1] = '\0';
        return 0;
    }

    return -1;
}

// For some unkown reason, libubsan makes vsomeip use a different symbol for gethostname
// so replace the behavior of that one too >:)
int __gethostname(char* name, size_t size) {
    return gethostname(name, size);
}

int __gethostname_chk(char* buf, size_t buflen, size_t maxlen) {
    if (maxlen < buflen)
        return -1; // ERANGE

    return gethostname(buf, buflen);
}
