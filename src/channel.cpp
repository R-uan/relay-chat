#include "channel.hpp"
#include "managers.hpp"
#include "server.hpp"
#include "spdlog/spdlog.h"
#include "utilities.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <format>
#include <optional>
#include <string>
#include <sys/types.h>
#include <vector>

/*Enters the channel.
 * - Check if the MAXCAPACITY has been reached.
 * - If the channel is secret, check if the client was invited.
 */
bool Channel::enter_channel(w_client w_client) {
  if (this->secret)
    if (std::erase_if(this->invitations, [&](int &invitation) {
          return invitation == w_client.lock()->id;
        }) == 0)
      return false;

  if (this->members.size() == this->MAXCAPACITY)
    return false;

  this->members.push_back(w_client);
  return true;
}

/* Disconnects a member from the channel.
 * The returned bool answers if the channel should be flagged for deletion or
 * not. It will only be true if no moderators are available when the emperor
 * leaves.
 */
bool Channel::disconnect_member(const w_client &w_client) {
  auto s_client = w_client.lock();
  std::unique_lock lock(this->mtx);
  // member leaving is the emperor and the channel...
  if (this->emperor.lock() == s_client) {
    // ...has no moderators so it will be deleted.
    if (this->moderators.empty())
      return true;
    // ...has at least one moderator that will take his place.
    auto new_emperor = this->moderators[0];
    this->moderators.erase(this->moderators.begin());
    this->emperor = new_emperor;
    // remove emperor from the member pool
    std::erase_if(this->members, [&](const ::w_client &w_client) {
      return w_client.lock() == s_client;
    });
    // won't be deleted
    return false;
  }

  // try to remove member from member pool
  std::erase_if(this->members, [&](const ::w_client &member) {
    return member.lock() == s_client;
  });

  // try to remove member from the moderator pool
  std::erase_if(this->moderators, [&](const ::w_client &w_client) {
    return w_client.lock() == s_client;
  });

  // member/moderator leaving shouldn't trigger deletion.
  return false;
}

Channel::Channel(int id, w_client creator, w_server server)
    : id(id), emperor(creator), server(server) {
  this->name = std::format("#channel{}", id);
  this->members.push_back(creator);
  spdlog::debug("channel created: {0}", this->name);

  // revise this later
  this->messageQueueWorkerThread = std::thread([this]() {
    while (!this->stopBroadcast) {
      std::unique_lock lock(this->queueMutex);
      this->cv.wait(lock, [this]() {
        return this->stopBroadcast || !this->messageQueue.empty();
      });
      if (this->stopBroadcast)
        return;

      if (!this->server.expired()) {
        this->server.lock()->threadPool->enqueue([this]() {
          std::vector<Response> messages_to_send;
          {
            std::unique_lock lock(this->queueMutex);
            while (!this->messageQueue.empty()) {
              messages_to_send.push_back(this->messageQueue.front());
              this->messageQueue.pop();
            }
          }
          for (const auto &packet : messages_to_send) {
            for (auto member : this->members) {
              if (auto client = member.lock()) {
                client->send_packet(packet);
              }
            }
          }
        });
      }
    }
  });
}

Channel::~Channel() {
  auto server = this->server.lock();
  auto data = std::format("{} destroyed", this->name);
  auto packet = c_response(0, DATAKIND::CH_COMMAND, data);
  // revise this later
  for (w_client pointer : this->members) {
    if (!pointer.expired()) {
      auto client = pointer.lock();
      client->remove_channel(this->id);
      if (client->connected) {
        server->threadPool->enqueue(
            [packet, client]() { client->send_packet(packet); });
      }
    }
  }

  this->stopBroadcast.exchange(true);
  this->cv.notify_all();

  if (this->messageQueueWorkerThread.joinable()) {
    this->messageQueueWorkerThread.join();
  }

  spdlog::debug("channel destroyed: {0}", this->name);
}

std::vector<char> Channel::info() {
  auto id = this->id;
  auto name = this->name;
  auto secret = this->secret ? 1 : 0;

  std::vector<char> information;
  information.reserve(5 + name.size());

  std::memcpy(information.data(), &id, sizeof(id));
  std::memcpy(information.data() + 4, &secret, 1);
  std::memcpy(information.data() + 5, name.data(), name.size());

  return information;
}

void Channel::broadcast(Response packet) {
  auto server = this->server.lock();
  server->threadPool->enqueue([&, this, packet]() {
    for (auto member : this->members) {
      if (auto client = member.lock()) {
        client->send_packet(packet);
      }
    }
  });
}

bool Channel::send_message(const w_client &wclient, std::string message) {
  std::vector<char> payload;
  auto client = wclient.lock();
  uint32_t channelId = this->id;
  uint32_t clientId = client->id;
  payload.resize(8 + message.size());

  std::memcpy(payload.data(), &channelId, sizeof(channelId));
  std::memcpy(payload.data() + 4, &clientId, sizeof(clientId));
  std::memcpy(payload.data() + 8, &message, message.size());

  Response packet = this->create_broadcast(DATAKIND::CH_MESSAGE, payload);
  std::unique_lock lock(this->queueMutex);
  this->messageQueue.push(packet);
  this->cv.notify_one();
  return true;
}

// UTILITIES

// Creates a response packet from a string.
Response Channel::create_broadcast(DATAKIND type, std::vector<char> data) {
  auto response = c_response(this->packetIds, type, data);
  this->packetIds.fetch_add(1);
  return response;
}

// Creates a response packet for a CH_COMMAND request.
Response Channel::create_broadcast(COMMAND command, std::string data) {
  std::vector<char> payload(data.size() + 1);
  payload[0] = command;
  std::memcpy(payload.data() + 1, data.data(), data.size());
  return this->create_broadcast(DATAKIND::CH_COMMAND, payload);
}

// Checks if the actor is a moderator or emperor
bool Channel::is_authority(const w_client &w_client) {
  auto target = w_client.lock();
  if (this->emperor.lock() == target)
    return true;
  auto it =
      std::find_if(this->moderators.begin(), this->moderators.end(),
                   [&](const ::w_client mo) { return mo.lock() == target; });
  return it != this->moderators.end();
}

// CH_COMMAND HANDLERS

// * Changes the secret status of the channel
// - Only the emperor can do this.
bool Channel::change_privacy(const w_client &w_client) {
  spdlog::debug("channel is now private: {0}", this->name);
  if (w_client.lock() == this->emperor.lock()) {
    this->secret.exchange(!this->secret);
    return true;
  }
  return false;
}

// * Kicks a member from the chanel
// - Only moderators can execute this command.
// - Only the emperor can kick other moderators.
bool Channel::kick_member(const w_client &w_client, int target_id) {
  if (this->is_authority(w_client)) {
    auto target_client =
        std::find_if(this->members.begin(), this->members.end(),
                     [&](const ::w_client member) {
                       return member.lock()->id == target_id;
                     });

    if (target_client == this->members.end())
      return false;

    if (this->is_authority(*target_client) &&
        w_client.lock() != this->emperor.lock())
      return false;
    spdlog::debug("member kicked from {0}", this->name);

    return disconnect_member(*target_client);
  }
  return false;
}

// * Invites a member to the channel.
// - If the channel is secret, only moderators can invite.
bool Channel::invite_member(const w_client &w_client, int target_id) {
  if (this->secret && !this->is_authority(w_client))
    return false;
  auto server = this->server.lock();
  auto new_member = server->clients->find_client(target_id);
  if (new_member != std::nullopt) {
    this->invitations.push_back(target_id);
    return true;
  }
  return false;
}

// * Promote member into a moderator.
// - Only the emperor can execute this command.
bool Channel::promote_member(const w_client &w_client, int target_id) {
  auto s_client = w_client.lock();
  if (s_client != this->emperor.lock() || this->moderators.size() == 5)
    return false;
  auto target_member = std::find_if(
      this->members.begin(), this->members.end(),
      [&](const ::w_client member) { return member.lock()->id == target_id; });
  if (target_member == this->members.end())
    return false;
  this->moderators.push_back(*target_member);
  spdlog::debug("member promoted to moderator: {0} -> {1}", this->name,
                target_member->lock()->username);
  return true;
}

// * Promotes a moderator into the emperor
// - Only the emperor can execute this command.
// - Member promotion will be broadcasted to the whole channel.
bool Channel::promote_moderator(const w_client &w_client, int target_id) {
  if (w_client.lock() != this->emperor.lock() || this->moderators.size() == 0)
    return false;

  auto moderator = std::find_if(
      this->moderators.begin(), this->moderators.end(),
      [&](const ::w_client &client) { return client.lock()->id == target_id; });

  if (moderator == this->members.end())
    return false;

  auto emperor = this->emperor;
  this->emperor = *moderator;
  this->moderators.push_back(emperor);

  std::erase_if(this->moderators, [&](const ::w_client &client) {
    return client.lock()->id == target_id;
  });

  spdlog::debug("moderator promoted to emperor: {0} -> {1}", this->name,
                this->emperor.lock()->username);
  return true;
}

// * Pins a message on the server to be seen by all members.
// - Only moderators can execute this command.
// - The message will be broadcasted to the whole channel.
bool Channel::pin_message(const w_client &w_client, std::string message) {
  if (this->is_authority(w_client)) {
    {
      std::unique_lock lock(this->mtx);
      this->pinnedMessage = message;
    }
    auto packet = this->create_broadcast(COMMAND::PIN, message);
    this->broadcast(packet);
    return true;
  }

  return false;
}

// * Changes the channel name.
// - Only the emperor can execute this.
// - The new name can have between 6-24 characters.
// - The new name will be broadcasted to the whole channel.
bool Channel::set_channel_name(const w_client &w_client, std::string new_name) {
  if (this->emperor.lock() == w_client.lock()) {
    spdlog::debug("channel name changed: {0} -> {1}", this->name, new_name);
    {
      std::unique_lock lock(this->mtx);
      this->name = new_name;
    }
    auto packet = this->create_broadcast(COMMAND::RENAME, new_name);
    this->broadcast(packet);
    return true;
  }

  return false;
}
