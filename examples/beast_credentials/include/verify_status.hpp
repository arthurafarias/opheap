#pragma once

namespace credentials {

// `rejected` is returned for an unknown principal, a disabled credential, and
// a wrong secret alike -- the caller must not be able to distinguish them.
enum class verify_status { ok, rejected, rate_limited };

} // namespace credentials
