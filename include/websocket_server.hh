#pragma once

#include "server.hh"
#include "typedef.hh"
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

namespace websocketpp {
namespace lib {
namespace asio {
#ifndef WEBSOCKETPP_ASIO_IO_SERVICE
using io_service = io_context;
#endif
} // namespace asio
} // namespace lib
} // namespace websocketpp

struct WebSocketServer {
private:
  websocket_server ws_server_;
  std::mutex connections_mtx_;
  std::shared_ptr<Server> tcp_server_;
  std::set<connection_ptr> connections_;
  std::map<ws_handle, int, std::owner_less<ws_handle>> handle_to_id_;

public:
  void stop();
  void run(uint16_t port);

  WebSocketServer(std::shared_ptr<Server> server);

private:
  void on_open(websocketpp::connection_hdl hdl);
  void on_close(websocketpp::connection_hdl hdl);
  void on_message(websocketpp::connection_hdl hdl, message_ptr msg);
};
