#include "channel.hh"
#include "client.hh"
#include "spdlog/spdlog.h"
#include "thread_pool.hh"
#include "typedef.hh"
#include "utilities.hh"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <format>
#include <string>
#include <sys/types.h>
#include <vector>

Channel::Channel(uint32_t id, std::string name) : id(id), name(name) {
  spdlog::debug("channel created: {0}", this->name);
  this->messageQueueWorkerThread = std::thread([this]() {
    while (!this->stopBroadcast) {
      std::unique_lock lock(this->queueMutex);
      this->cv.wait(lock, [this]() {
        return this->stopBroadcast || !this->messageQueue.empty();
      });
      if (this->stopBroadcast)
        return;

      ThreadPool::initialize().enqueue([this]() {
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
  });
}

Channel::~Channel() {
  auto data = std::format("{} has been deleted", this->name);
  auto packet = response(0, CH_DELETE, data);

  //
  auto &thread_pool = ThreadPool::initialize();
  for (w_client w_client : this->members) {
    if (!w_client.expired()) {
      auto s_client = w_client.lock();
      s_client->remove_channel(this->id);
      if (s_client->connected) {
        thread_pool.enqueue(
            [packet, s_client]() { s_client->send_packet(packet); });
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

/* Attempt to add a member to the channel.
 *
 * Check if the MAXCAPACITY has been reached.
 *
 * If the channel is secret, check if the client was invited.
 */
JOINRESULT Channel::join_channel(const w_client &w_client) {
  auto s_client = w_client.lock();
  auto is_banned = std::find_if(this->banned.begin(), this->banned.end(),
                                [&](int id) { return id == s_client->id; });
  if (is_banned != this->banned.end())
    return JOINRESULT::BANNED;

  // capacity check before secrecy so invitation doesn't get deleted on full
  // server
  if (this->members.size() >= this->MAXCAPACITY) {
    return JOINRESULT::FULL;
  }

  // if no invitation was deleted that means the client wasn't invited
  if (this->secret && std::erase_if(this->invitations, [&](int id) {
                        return id == s_client->id;
                      }) == 0) {
    return JOINRESULT::SECRET;
  }

  this->members.push_back(w_client);

  return JOINRESULT::SUCCESS;
}

/* Disconnects a member from the channel.
 * The returned bool answers if the channel should be flagged for deletion or
 * not. It will only be true if no moderators are available when the emperor
 * leaves.
 */
void Channel::leave_channel(const w_client &w_client) {
  auto s_client = w_client.lock();
  std::unique_lock lock(this->mtx);
  // try to remove member from member pool
  std::erase_if(this->members, [&](const ::w_client &member) {
    return member.lock() == s_client;
  });

  // try to remove member from the moderator pool
  std::erase_if(this->moderators, [&](const ::w_client &w_client) {
    return w_client.lock() == s_client;
  });
}

std::vector<char> Channel::info() {
  uint32_t id = this->id;
  auto name = this->name;
  auto secret = this->secret ? 1 : 0;

  std::vector<char> information;
  information.reserve(5 + name.size());

  std::memcpy(information.data(), &id, sizeof(id));
  std::memcpy(information.data() + 4, &secret, 1);
  std::memcpy(information.data() + 5, name.data(), name.size());

  return information;
}

void Channel::queue_message(const MessageView view) {
  auto channel_id = view.channel_id;
  auto client_id = view.sender_id;
  auto reply_to = view.reply_to;
  auto message = view.message;

  std::vector<char> payload;
  payload.resize(12 + message.size());

  std::memcpy(payload.data(), &channel_id, sizeof(channel_id));
  std::memcpy(payload.data() + 4, &client_id, sizeof(client_id));
  std::memcpy(payload.data() + 8, &reply_to, sizeof(reply_to));
  std::memcpy(payload.data() + 12, &message, message.size());

  Response packet = response(this->packetIds, CH_MESSAGE, payload);
  std::unique_lock lock(this->queueMutex);
  this->messageQueue.push(packet);
  this->packetIds.fetch_add(1);
  this->cv.notify_one();
}

// UTILITIES

// Checks if the actor is a moderator or emperor
bool Channel::is_moderator(const w_client &w_client) {
  auto target = w_client.lock();
  auto it =
      std::find_if(this->moderators.begin(), this->moderators.end(),
                   [&](const ::w_client mo) { return mo.lock() == target; });
  return it != this->moderators.end() || target->admin;
}

// CH_COMMAND HANDLERS

/* Changes the secret status of the channel
 */
MODERATIONRESULT Channel::change_privacy(const w_client &w_client) {
  spdlog::debug("{} privacy has changed", this->name);
  if (w_client.lock()->admin) {
    this->secret.exchange(!this->secret);
    return MODERATIONRESULT::SUCCESS;
  }
  return MODERATIONRESULT::UNAUTHORIZED;
}

/* Kicks a member from the channel
 * - Only moderators can execute this command.
 */
MODERATIONRESULT Channel::kick_member(const w_client &wclient, int target_id) {
  auto target = std::find_if(
      this->members.begin(), this->members.end(),
      [&](const ::w_client member) { return member.lock()->id == target_id; });

  if (target == this->members.end())
    return MODERATIONRESULT::NOT_FOUND;

  if ((this->is_moderator(*target) && !wclient.lock()->admin) ||
      !this->is_moderator(wclient))
    return MODERATIONRESULT::UNAUTHORIZED;

  spdlog::debug("{} was kicked from: {}", target->lock()->username, this->name);
  this->leave_channel(*target);
  return MODERATIONRESULT::SUCCESS;
}

/* Add a client to the invitation list
 * Only authorities can invite to private servers
 */
MODERATIONRESULT Channel::promote_member(const w_client &w_client,
                                         int target_id) {
  auto s_client = w_client.lock();
  if (!s_client->admin)
    return MODERATIONRESULT::UNAUTHORIZED;

  auto target_member = std::find_if(
      this->members.begin(), this->members.end(),
      [&](const ::w_client member) { return member.lock()->id == target_id; });

  if (target_member == this->members.end())
    return MODERATIONRESULT::NOT_FOUND;

  this->moderators.push_back(*target_member);
  spdlog::debug("member promoted to moderator: {0} -> {1}", this->name,
                target_member->lock()->username);
  return MODERATIONRESULT::SUCCESS;
}

ChannelView::ChannelView(Channel *channel) {
  this->id = &channel->id;
  this->name = &channel->name;
  this->secret = channel->secret;
}

ChannelView Channel::get_view() { return ChannelView(this); }
