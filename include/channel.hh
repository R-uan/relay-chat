#pragma once

#include "typedef.hh"
#include "utilities.hh"
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <queue>
#include <thread>
#include <vector>

enum class JOINRESULT { SUCCESS = 0, BANNED, SECRET, FULL };
enum class MODERATIONRESULT { SUCCESS, NOT_FOUND, UNAUTHORIZED };

struct ChannelView {
  bool secret;
  const uint32_t *id;
  const std::string *name;

  ChannelView(Channel *channel);
};

struct MessageView {
  const uint32_t sender_id;
  const uint32_t channel_id;
  const uint32_t reply_to;
  const std::string message;

  MessageView(uint32_t sender, uint32_t channel, uint32_t reply_to,
              std::string message)
      : sender_id(sender), channel_id(channel), reply_to(reply_to),
        message(message) {}
};

/* Each channel HAS an emperor and CAN HAVE up to five moderators.
 * - emperor : the one that created the channel by joining it first.
 * - moderators : assigned users by the emperor to have elevated privileges.
 *
 * If the emperor leaves the channel, the oldest moderator will take it's place.
 * If there is no moderator, the channel will be destroyed.
 * Emperor can manually promote a moderator to emperor, swapping their roles.
 *
 * If channel is secret, chatters can only join by being invited by a moderator.
 * An invitation token is created by a moderator to send to a chatter.
 *
 * The invited chatter should send the token with the enter request.
 */
class Channel {
public:
  uint32_t id;
  std::mutex mtx;
  std::string name;
  const size_t MAXCAPACITY{50};

  std::atomic_int packetIds{1};
  std::atomic_bool secret{false};

  std::string pinnedMessage;
  std::vector<int> banned{};
  std::vector<int> invitations{};
  std::vector<w_client> members{};
  std::vector<w_client> moderators{};

  std::mutex queueMutex;
  std::condition_variable cv;
  std::queue<Response> messageQueue{};
  std::thread messageQueueWorkerThread;
  std::atomic_bool stopBroadcast{false};

  // utils
  ChannelView get_view();
  std::vector<char> info();
  bool is_moderator(const w_client &w_client); // *

  void queue_message(const MessageView view);

  void leave_channel(const w_client &target_id);     // *
  JOINRESULT join_channel(const w_client &w_client); // *

  MODERATIONRESULT change_privacy(const w_client &w_client);
  MODERATIONRESULT kick_member(const w_client &w_client, int target_id);
  MODERATIONRESULT invite_member(const w_client &w_client, int target_id);
  MODERATIONRESULT promote_member(const w_client &w_client, int target_id);

  Channel(uint32_t id, std::string name);

  ~Channel();
};
