#pragma once
#include <memory>
#include <string_view>
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/roles/server_endpoint.hpp>

class Server;
class Client;
class Channel;

using w_client = std::weak_ptr<Client>;
using w_server = std::weak_ptr<Server>;

using websocket_server = websocketpp::server<websocketpp::config::asio>;
using message_ptr = websocketpp::config::asio::message_type::ptr;
using connection_ptr = websocket_server::connection_ptr;
using ws_handle = websocketpp::connection_hdl;

constexpr std::string_view INVALID_PACKET = "Request packet was invalid.";
