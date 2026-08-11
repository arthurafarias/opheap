#include "http_router.hpp"

#include <boost/json.hpp>

#include <exception>
#include <string>

namespace credentials {

namespace {

namespace http = boost::beast::http;
namespace json = boost::json;

http::response<http::string_body> make_response(http::status status,
                                                 const http::request<http::string_body>& request,
                                                 json::value body) {
    http::response<http::string_body> response{status, request.version()};
    response.set(http::field::server, "opheap-beast-credentials-example");
    response.set(http::field::content_type, "application/json");
    response.keep_alive(request.keep_alive());
    response.body() = json::serialize(body);
    response.prepare_payload();
    return response;
}

http::response<http::string_body> make_error(http::status status,
                                              const http::request<http::string_body>& request,
                                              std::string_view message) {
    return make_response(status, request, json::object{{"error", std::string{message}}});
}

bool admin_authorized(const http::request<http::string_body>& request, std::string_view admin_token) {
    auto it = request.find("X-Admin-Token");
    return it != request.end() && it->value() == admin_token;
}

// Parses {"principal": string, "secret": string} out of a request body.
// Returns false (and writes an error response into `out_error`) on any
// malformed input, so callers can bail out with a single check.
bool parse_principal_secret(const http::request<http::string_body>& request, std::string& principal,
                            std::string& secret, http::response<http::string_body>& out_error) {
    json::value parsed;
    try {
        parsed = json::parse(request.body());
    } catch (const std::exception&) {
        out_error = make_error(http::status::bad_request, request, "invalid json");
        return false;
    }
    if (!parsed.is_object()) {
        out_error = make_error(http::status::bad_request, request, "expected a json object");
        return false;
    }
    const auto& body = parsed.as_object();
    const auto* principal_field = body.if_contains("principal");
    const auto* secret_field = body.if_contains("secret");
    if (!principal_field || !principal_field->is_string() || !secret_field || !secret_field->is_string()) {
        out_error = make_error(http::status::bad_request, request, "principal and secret are required strings");
        return false;
    }
    principal = std::string{principal_field->as_string()};
    secret = std::string{secret_field->as_string()};
    return true;
}

http::response<http::string_body> handle_provision(credential_service& service,
                                                    const http::request<http::string_body>& request,
                                                    std::string_view admin_token, std::string_view client_key) {
    if (!admin_authorized(request, admin_token)) {
        return make_error(http::status::unauthorized, request, "unauthorized");
    }

    json::value parsed;
    try {
        parsed = json::parse(request.body());
    } catch (const std::exception&) {
        return make_error(http::status::bad_request, request, "invalid json");
    }
    if (!parsed.is_object()) return make_error(http::status::bad_request, request, "expected a json object");
    const auto& body = parsed.as_object();
    const auto* principal_field = body.if_contains("principal");
    const auto* secret_field = body.if_contains("secret");
    if (!principal_field || !principal_field->is_string() || !secret_field || !secret_field->is_string()) {
        return make_error(http::status::bad_request, request, "principal and secret are required strings");
    }
    bool disabled = false;
    if (const auto* disabled_field = body.if_contains("disabled"); disabled_field && disabled_field->is_bool()) {
        disabled = disabled_field->as_bool();
    }

    const auto status = service.provision(principal_field->as_string(), secret_field->as_string(), disabled,
                                          client_key);
    switch (status) {
        case provision_status::ok:
            return make_response(http::status::ok, request, json::object{{"status", "ok"}});
        case provision_status::invalid_principal:
            return make_error(http::status::bad_request, request, "invalid principal");
        case provision_status::invalid_secret:
            return make_error(http::status::bad_request, request, "secret does not meet policy");
        case provision_status::rate_limited:
            return make_error(http::status::too_many_requests, request, "rate limited");
    }
    return make_error(http::status::internal_server_error, request, "unreachable");
}

http::response<http::string_body> handle_verify(credential_service& service,
                                                 const http::request<http::string_body>& request,
                                                 std::string_view client_key) {
    std::string principal;
    std::string secret;
    http::response<http::string_body> error_response{http::status::bad_request, request.version()};
    if (!parse_principal_secret(request, principal, secret, error_response)) return error_response;

    const auto status = service.verify(principal, secret, client_key);
    switch (status) {
        case verify_status::ok:
            return make_response(http::status::ok, request, json::object{{"status", "ok"}});
        case verify_status::rejected:
            // Same status and body for "unknown principal", "disabled" and
            // "wrong secret" -- see docs/credentials-service.md.
            return make_error(http::status::unauthorized, request, "invalid credentials");
        case verify_status::rate_limited:
            return make_error(http::status::too_many_requests, request, "rate limited");
    }
    return make_error(http::status::internal_server_error, request, "unreachable");
}

http::response<http::string_body> handle_remove(credential_service& service,
                                                 const http::request<http::string_body>& request,
                                                 std::string_view admin_token, std::string_view client_key) {
    if (!admin_authorized(request, admin_token)) {
        return make_error(http::status::unauthorized, request, "unauthorized");
    }
    constexpr std::string_view prefix = "/v1/credentials/";
    const auto principal = std::string{request.target().substr(prefix.size())};
    if (principal.empty()) return make_error(http::status::bad_request, request, "principal is required");

    const auto status = service.remove(principal, client_key);
    switch (status) {
        case remove_status::ok:
            return make_response(http::status::ok, request, json::object{{"status", "ok"}});
        case remove_status::rate_limited:
            return make_error(http::status::too_many_requests, request, "rate limited");
    }
    return make_error(http::status::internal_server_error, request, "unreachable");
}

} // namespace

http::response<http::string_body> handle_request(credential_service& service,
                                                  const http::request<http::string_body>& request,
                                                  std::string_view admin_token, std::string_view client_key) {
    const auto target = request.target();
    const auto method = request.method();

    if (method == http::verb::get && target == "/healthz") {
        return make_response(http::status::ok, request, json::object{{"status", "ok"}});
    }
    if (method == http::verb::post && target == "/v1/credentials") {
        return handle_provision(service, request, admin_token, client_key);
    }
    if (method == http::verb::post && target == "/v1/credentials/verify") {
        return handle_verify(service, request, client_key);
    }
    if (method == http::verb::delete_ && target.starts_with("/v1/credentials/")) {
        return handle_remove(service, request, admin_token, client_key);
    }
    return make_error(http::status::not_found, request, "not found");
}

} // namespace credentials
