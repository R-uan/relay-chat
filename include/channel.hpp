#pragma once

#include "managers.hpp"
#include "utilities.hpp"
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <queue>
#include <string_view>
#include <thread>
#include <vector>

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
  const int id;
  std::mutex mtx;
  std::string name;
  w_client emperor;
  const size_t MAXCAPACITY{50};

  w_server server;
  std::atomic_int packetIds{1};
  std::atomic_bool secret{false};

  std::string pinnedMessage;
  std::vector<int> invitations{};
  std::vector<w_client> members{};
  std::vector<w_client> moderators{};

  std::mutex queueMutex;
  std::condition_variable cv;
  std::queue<Response> messageQueue{};
  std::thread messageQueueWorkerThread;
  std::atomic_bool stopBroadcast{false};

  void broadcast(Response packet);
  bool send_message(const w_client &w_client, const std::string message);

  bool enter_channel(w_client w_client);             // *
  bool disconnect_member(const w_client &target_id); // *

  // utils
  std::vector<char> info();
  bool is_authority(const w_client &w_client); // *
  void self_destroy(std::string_view reason);  // *

  Response create_broadcast(COMMAND command, std::string data);
  Response create_broadcast(DATAKIND type, std::vector<char> data);

  // CH_COMMAND HANDLERS (Implementations [7/7])
  bool change_privacy(const w_client &w_client);
  bool kick_member(const w_client &w_client, int target_id);
  bool invite_member(const w_client &w_client, int target_id);
  bool promote_member(const w_client &w_client, int target_id);
  bool promote_moderator(const w_client &w_client, int target_id);
  bool pin_message(const w_client &w_client, std::string message);
  bool set_channel_name(const w_client &w_client, std::string newName);

  Channel(int id, w_client creator, w_server server);

  ~Channel();
};
