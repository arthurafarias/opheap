#pragma once

#include "credential_service.hpp"

#include <boost/beast/http.hpp>

#include <string_view>

// HTTP transport for the credential service. This is the only translation
// unit in the example that depends on Boost -- credential_service itself
// never sees Boost types, matching the layering in
// docs/credentials-service.md.
namespace credentials {

boost::beast::http::response<boost::beast::http::string_body> handle_request(
    credential_service& service,
    const boost::beast::http::request<boost::beast::http::string_body>& request,
    std::string_view admin_token,
    std::string_view client_key);

} // namespace credentials
