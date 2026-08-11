#include "credential_service.hpp"
#include "http_router.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/system/error_code.hpp>

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <thread>

namespace {

namespace beast = boost::beast;
namespace http = beast::http;
using boost::asio::ip::tcp;

std::string client_key_for(const tcp::socket& socket) {
    boost::system::error_code ec;
    const auto endpoint = socket.remote_endpoint(ec);
    return ec ? std::string{"unknown"} : endpoint.address().to_string();
}

void run_session(tcp::socket socket, credentials::credential_service& service, std::string admin_token) {
    const auto client_key = client_key_for(socket);
    beast::flat_buffer buffer;
    beast::error_code ec;

    for (;;) {
        http::request<http::string_body> request;
        http::read(socket, buffer, request, ec);
        if (ec == http::error::end_of_stream) break;
        if (ec) {
            std::cerr << "read error from " << client_key << ": " << ec.message() << '\n';
            break;
        }

        const auto response = credentials::handle_request(service, request, admin_token, client_key);
        const bool keep_alive = response.keep_alive();
        http::write(socket, response, ec);
        if (ec) {
            std::cerr << "write error to " << client_key << ": " << ec.message() << '\n';
            break;
        }
        if (!keep_alive) break;
    }
    socket.shutdown(tcp::socket::shutdown_send, ec);
}

} // namespace

int main(int argc, char** argv) {
    unsigned short port = 8080;
    auto storage_path = std::filesystem::temp_directory_path() / "opheap-beast-credentials";
    std::string admin_token = "change-me-in-production";

    for (int i = 1; i < argc; ++i) {
        const std::string_view arg{argv[i]};
        if (arg == "--port" && i + 1 < argc) {
            port = static_cast<unsigned short>(std::stoi(argv[++i]));
        } else if (arg == "--storage" && i + 1 < argc) {
            storage_path = argv[++i];
        } else if (arg == "--admin-token" && i + 1 < argc) {
            admin_token = argv[++i];
        } else {
            std::cerr << "usage: " << argv[0]
                      << " [--port N] [--storage PATH] [--admin-token TOKEN]\n";
            return 1;
        }
    }
    if (const char* env_token = std::getenv("CREDENTIAL_SERVICE_ADMIN_TOKEN")) admin_token = env_token;

    try {
        credentials::credential_service service{credentials::service_config{.storage_path = storage_path}};

        boost::asio::io_context io_context;
        tcp::acceptor acceptor{io_context, tcp::endpoint{tcp::v4(), port}};
        std::cout << "opheap beast_credentials listening on port " << acceptor.local_endpoint().port()
                  << ", storage at " << storage_path << '\n';
        if (admin_token == "change-me-in-production") {
            std::cout << "warning: using the example's default admin token; pass --admin-token for anything real\n";
        }

        for (;;) {
            tcp::socket socket{io_context};
            acceptor.accept(socket);
            std::thread{[&service, admin_token, sock = std::move(socket)]() mutable {
                run_session(std::move(sock), service, admin_token);
            }}.detach();
        }
    } catch (const std::exception& ex) {
        std::cerr << "fatal: " << ex.what() << '\n';
        return 1;
    }
}
