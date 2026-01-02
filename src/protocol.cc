#include "protocol.hh"
#include "managers.hh"
#include "typedef.hh"
#include "utilities.hh"
#include <spdlog/spdlog.h>
#include <string>

Response Protocol::handle_request(const std::shared_ptr<Client> s_client,
                                  const Request &request) {
  if (!s_client->connected) {
    if (SVR_CONNECT != request.type) {
      spdlog::debug("not connect request {}", s_client->id);
      return response(-1, SVR_CONNECT, (std::string) "Connection needed");
    }
    return Protocol::handle_server_connection(s_client, request);
  }

  switch (request.type) {
  default:
    return response(-1, ERROR, (std::string) "unknown request type");
  }
}

Response Protocol::handle_server_connection(const w_client w_client,
                                            const Request &request) {
  auto s_client = w_client.lock();
  auto payload = request.payload;
  std::string username(payload.begin(), payload.end());
  auto fmtusername = s_client->change_username(username);
  spdlog::debug("{} connected", fmtusername);
  s_client->set_connection(true);
  return response(request.id, SVR_CONNECT, fmtusername);
}

/* Removes the client accross the application by lowering the shared_ptr
 * counter to zero.
 *
 * Pointer tracker:
 *  - Server  -> clients::unordered_map
 *  - Channel -> chatters::vector
 *  - Channel -> moderators::vector
 *  - Channel -> emperor::shared_ptr
 */
void Protocol::server_disconnect(const w_client &w_client) {
  auto s_client = w_client.lock();
  auto &channel_ctx = ChannelManager::instance();
  auto &client_ctx = ClientManager::instance();
  s_client->connected.exchange(false);
  // loop over the client's connected channels
  // find said channels
  // diconnect client from it
  for (int id : s_client->channels) {
    auto channel = channel_ctx.find_channel(id);
    if (channel != nullptr) {
      // if the disconnect_member returns true it means the channel must be
      // deleted
      if (channel->remove_member(s_client)) {
        spdlog::debug("{0} flagged for deletion", channel->name);
        channel_ctx.remove_channel(id);
      }
    }
  }

  if (s_client->fd == -1) {
    client_ctx.remove_client(s_client->ws_hld.value());
  } else {
    client_ctx.remove_client(s_client->fd);
  }
  spdlog::info("{0} disconnected from the server", s_client->username);
}

/* Request to join a channel.
 * The JOIN packet payload will be composed of: <flag> \n <channel> \n <token>
 * - <flag>    : channel creation flag
 * - <channel> : target channel's id (int) to join.
 * - <token>   : invitation token (optional).
 */
Response Protocol::channel_join_request(const w_client &w_client,
                                        const Request &request) {
  auto &ctx = ChannelManager::instance();
  auto body = request.payload;
  if (body.size() < 5)
    return ::response(-1, CH_JOIN, INVALID_PACKET);

  bool creation_flag = body[0] == 1; // create channel if doesn't exist ?
  int channel_id = i32_from_le({body[1], body[2], body[3], body[4]});
  auto channel = ctx.find_channel(channel_id);

  if (channel == nullptr) { // channel doesn't exist...
    // ...and will be created
    if (creation_flag && ctx.has_capacity()) {
      auto info = ctx.create_channel(channel_id, w_client);
      return ::response(request.id, CH_JOIN, info);
    }
    // ...and won't be created
    return ::response(-1, CH_JOIN);
  } else { // channel exists and the client...
    // ...successfully joined
    std::string fail_reason;
    auto result = channel->add_member(w_client);
    switch (result) {
    case JOINRESULT::BANNED:
      fail_reason =
          std::format("You are banned from channel {}", channel->name);
      break;
    case JOINRESULT::FULL:
      fail_reason = std::format("Channel is full: {}", channel->name);
      break;
    case JOINRESULT::SECRET:
      fail_reason = std::format(
          "You need an invitation to join this channel: {}", channel->name);
      break;
    case JOINRESULT::SUCCESS:
      auto s_client = w_client.lock();
      s_client->add_channel(channel_id);
      auto channel_info = channel->info();
      spdlog::debug("{0} joined {1}", s_client->username, channel->name);
      return ::response(request.id, CH_JOIN, channel_info);
    }
    // ...couldn't join
    return ::response(-1, CH_JOIN);
  }
}

/* Disconnects the client from the channel.
 * - If channel may be flagged for deletion.
 */
Response Protocol::channel_disconnect(const w_client &w_client,
                                      const Request &request) {
  if (request.payload.size() >= 4) {
    auto &ctx = ChannelManager::instance();
    auto payload = request.payload;
    auto channel_id = i32_from_le(payload);
    auto channel = ctx.find_channel(channel_id);

    if (channel != nullptr) {
      auto s_client = w_client.lock();
      s_client->remove_channel(channel_id);
      spdlog::debug("{0} left {1}", s_client->username, channel->name);

      if (channel->remove_member(w_client)) {
        spdlog::debug("{0} flagged for deletion", channel->name);
        ctx.remove_channel(channel_id);
      }

      return ::response(request.id, CH_LEAVE);
    }
  }

  return ::response(-1, CH_LEAVE);
}

/* Sends message in a channel.
 * - Checks if the channel exists
 * - Checks if the client is in the channel.
 */
Response Protocol::channel_message_request(const w_client &w_client,
                                           const Request &request) {
  auto &ctx = ChannelManager::instance();
  const auto payload = request.payload;
  const uint32_t channel_id = i32_from_le(payload);
  const std::string message(payload.begin() + 4, payload.end());
  const auto channel = ctx.find_channel(channel_id);

  if (channel != nullptr) {
    if (w_client.lock()->is_member(channel_id)) {
      channel->send_message(w_client, message);
      return ::response(request.id, CH_MESSAGE);
    }
  }

  return ::response(-1, CH_MESSAGE);
}
