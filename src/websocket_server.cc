#include "websocket_server.hh"
#include "client.hh"
#include "managers.hh"
#include "protocol.hh"
#include "thread_pool.hh"
#include "typedef.hh"
#include "utilities.hh"
#include <cstdint>
#include <memory>
#include <optional>
#include <spdlog/spdlog.h>
#include <websocketpp/common/connection_hdl.hpp>

void WebSocketServer::run(uint16_t port) {
  this->ws_server_.listen(port);
  this->ws_server_.start_accept();
  spdlog::info("Websocket server listening on port {}", port);
  this->ws_server_.run();
}

void WebSocketServer::stop() {}

WebSocketServer::WebSocketServer(std::shared_ptr<Server> server)
    : tcp_server_(server) {
  using std::placeholders::_1;
  using std::placeholders::_2;

  this->ws_server_.set_open_handler(
      std::bind(&WebSocketServer::on_open, this, _1));

  this->ws_server_.set_close_handler(
      std::bind(&WebSocketServer::on_close, this, _1));

  this->ws_server_.set_message_handler(
      std::bind(&WebSocketServer::on_message, this, _1, _2));

  this->ws_server_.init_asio();
  this->ws_server_.set_reuse_addr(true);
}

void WebSocketServer::on_open(ws_handle hdl) {
  spdlog::info("on_open handler called!"); // Change to info temporarily
  auto &ctx = ClientManager::instance();
  auto clientId = ctx.add_client(hdl);
  this->handle_to_id_.emplace(hdl, clientId);
  spdlog::debug("new websocket client connected:");
}

void WebSocketServer::on_close(ws_handle hdl) {
  auto &ctx = ClientManager::instance();
  auto fclient = ctx.find_client(hdl);
  if (fclient == std::nullopt)
    return;

  w_client s_client = fclient.value();
  ThreadPool::initialize().enqueue(
      [s_client]() { Protocol::server_disconnect(s_client); });
}

void WebSocketServer::on_message(ws_handle hdl, message_ptr msg) {
  auto &ctx = ClientManager::instance();
  auto conn = this->ws_server_.get_con_from_hdl(hdl);

  std::shared_ptr<Client> s_client;
  {
    auto fclient = ctx.find_client(hdl);
    if (fclient == std::nullopt)
      return;
    s_client = fclient.value();
  }

  auto payload = msg->get_payload();
  std::vector<uint8_t> buffer(payload.begin() + 4, payload.end());
  Request request(buffer);

  auto response = Protocol::handle_request(s_client, request);

  try {
    ws_server_.send(hdl, response.data.data(), response.data.size(),
                    websocketpp::frame::opcode::binary);
  } catch (const std::exception &e) {
    spdlog::error("WebSocket send failed: {}", e.what());
  }
}
