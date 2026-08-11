#pragma once

namespace credentials {

enum class provision_status { ok, invalid_principal, invalid_secret, rate_limited };

} // namespace credentials
