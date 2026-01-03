#include "protocol.hh"
#include "managers.hh"
#include "typedef.hh"
#include "utilities.hh"
#include <cstdint>
#include <spdlog/spdlog.h>
#include <string>
#include <vector>

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
  case (uint32_t)CH_LIST:
    spdlog::debug("CH_LIST request");
    return Protocol::list_channels_request(request);
  case (uint32_t)CH_CREATE:
    spdlog::debug("CH_CRETE request");
    if (s_client->admin)
      return Protocol::create_channel_request(request);
    return response(-1, PERMISSION_DENIED);
  case (uint32_t)CH_MESSAGE:
    spdlog::debug("CH_MESSAGE request");
    return Protocol::channel_message_request(s_client, request);
  default:
    spdlog::debug("Unknown request type: {}", request.type);
    return response(-1, ERROR, (std::string) "unknown request type");
  }
}

Response Protocol::handle_server_connection(const w_client w_client,
                                            const Request &request) {
  auto s_client = w_client.lock();
  auto payload = split(request.payload, '\n');
  auto username = s_client->change_username(payload[0]);
  s_client->set_connection(true);

  if (payload.size() == 2)
    s_client->set_admin(payload[1]);

  return response(request.id, SVR_CONNECT, username);
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
      channel->leave_channel(s_client);
      spdlog::debug("{0} flagged for deletion", channel->name);
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
 */
Response Protocol::channel_join_request(const w_client &w_client,
                                        const Request &request) {
  auto payload = request.payload;
  int channel_id = i32_from_le(payload);
  auto &ctx = ChannelManager::instance();
  auto channel = ctx.find_channel(channel_id);

  if (channel == nullptr) { // channel doesn't exist...
    return response(-1, NOT_FOUND, (std::string) "Channel not found.");
  } else { // channel exists and the client...
    auto result = channel->join_channel(w_client);
    std::string fr;

    switch (result) {
    case JOINRESULT::BANNED:
      fr = std::format("You are banned from channel {}", channel->name);
      break;
    case JOINRESULT::FULL:
      fr = std::format("Channel is full: {}", channel->name);
      break;
    case JOINRESULT::SECRET:
      fr = std::format("You need an invitation to join this channel: {}",
                       channel->name);
      break;
    case JOINRESULT::SUCCESS:
      auto s_client = w_client.lock();
      s_client->add_channel(channel_id);
      auto channel_info = channel->info();
      spdlog::debug("{0} joined {1}", s_client->username, channel->name);
      return ::response(request.id, CH_JOIN, channel_info);
    }
    return ::response(-1, CH_JOIN, fr);
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
      channel->leave_channel(w_client);
      return ::response(request.id, CH_LEAVE);
    }
  }

  return ::response(-1, CH_LEAVE);
}

/* Sends message in a channel.
 * - Checks if the channel exists
 * - Checks if the client is in the channel.
 * - Incoming
 *    Message {
 *      channelId = 4 bytes
 *      replyTo = 4 bytes
 *      message = ascii string
 *    }
 */
Response Protocol::channel_message_request(const w_client &w_client,
                                           const Request &request) {
  auto &ctx = ChannelManager::instance();

  const auto payload = request.payload;

  const auto channel_id = i32_from_le(payload);
  const auto reply_to =
      i32_from_le({payload[4], payload[5], payload[6], payload[7]});
  const std::string message(payload.begin() + 8, payload.end());
  const auto channel = ctx.find_channel(channel_id);

  if (channel != nullptr) {
    auto s_client = w_client.lock();
    MessageView msg_view(s_client->id, channel_id, reply_to, message);
    if (s_client->is_member(channel_id)) {
      channel->queue_message(msg_view);
      return ::response(request.id, CH_MESSAGE);
    }
  }

  return ::response(-1, CH_MESSAGE);
}

Response Protocol::create_channel_request(const Request &request) {
  auto payload = request.payload;
  auto &ctx = ChannelManager::instance();
  std::string channel_name(payload.begin() + 1, payload.end());
  bool secret = static_cast<int>(payload[0]) == 1;
  auto info = ctx.create_channel(channel_name, secret);
  return response(request.id, CH_CREATE, info);
}

Response Protocol::list_channels_request(const Request &request) {
  auto &channel_manager = ChannelManager::instance();
  auto views = channel_manager.get_views();
  std::vector<uint8_t> views_bytes;
  for (auto view : views) {
    std::string id_str = std::to_string(*view.id);
    views_bytes.insert(views_bytes.end(), id_str.begin(), id_str.end());
    views_bytes.push_back('\n');

    views_bytes.push_back(view.secret);
    views_bytes.push_back('\n');

    views_bytes.insert(views_bytes.end(), view.name->begin(), view.name->end());
    views_bytes.push_back('\n');

    // View separator (null byte)
    views_bytes.push_back(0x00);
  }
  views_bytes.push_back(0x00);
  return response(request.id, CH_LIST, views_bytes);
}
