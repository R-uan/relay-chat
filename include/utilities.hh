#pragma once

#include <concepts>
#include <cstdint>
#include <cstring>
#include <sys/types.h>
#include <vector>

class Channel;

int i32_from_le(const std::vector<uint8_t> &bytes);
std::vector<std::vector<uint8_t>> split_newline(std::vector<uint8_t> &data);

enum class PACKET_TYPE : uint32_t {
  // If any of these fails they will have an ID of -1 and a reason payload.
  // client -> server : connects to the server
  // server -> client : connected.
  SVR_CONNECT = 0x01,
  // client -> server : disconnects from the whole server.
  // server -> client : idk he got disconnected.
  SVR_DISCONNECT = 0x02,
  // client -> server : attempt to send a server scoped message.
  // server -> client : message from the server.
  SVR_MESSAGE = 0x03,
  // client -> server : attempts to ban a client from the server.
  // server -> client : you've been banned.
  SVR_BANNED = 0x04,
  // client -> server : attempt to shutdown server
  // server -> client : server has been shutdown.
  SVR_SHUTDOWN = 0x05,
  // client -> server : attempt to join the channel
  // server -> client : a client has connected to the channel.
  CH_JOIN = 0x10,
  // client -> server : disconnects from a channel.
  // server -> client : a client has disconnected from the channel.
  CH_LEAVE = 0x11,
  // client -> server : send a message to the channel.
  // server -> client : breadcasted message from a channel.
  CH_MESSAGE = 0x12,
  // client -> server : attempt to update channel's info
  // server -> client : a channel's info has been updated.
  // contains bitmask telling what has been updated.
  CH_UPDATE = 0x13,
  // client -> server : attempt to delete a channel
  // server -> client : a chanel has been deleted
  CH_DELETE = 0x14,
  // client -> server : attempt to create channel
  // server -> client : channel has been created
  CH_CREATE = 0x15,
  // client -> server : request channel list.
  // server -> client : list of channels
  CH_LIST = 0x16,
  // client -> server : attempt to invite
  // server -> client : channel invitation
  CH_INVITE = 0x20,
  // client -> server : attempt to kick a client.
  // server -> client : a client has been kicked.
  CH_KICK = 0x21,
  // client -> server : attempt to ban a client.
  // server -> client : a client has been banned.
  CH_BAN = 0x22,
  // client -> server : attempt to unban client
  // server -> client : client was unbanned
  CH_UNBAN = 0x23,
  // server -> client : operation rejected (with reason code)
  REQUEST_REJECTED = 0xF0,
  // server -> client : you don't have permission
  PERMISSION_DENIED = 0xF1,
  // server -> client : requested resource not found
  NOT_FOUND = 0xF2,
  // client <-> server: heartbeat/keepalive
  HEARTBEAT = 0xFE,
  // server -> client : generic error (with error message)
  ERROR = 0xFF,
};

constexpr bool operator==(PACKET_TYPE lhs, uint32_t rhs) {
  return static_cast<uint32_t>(lhs) == rhs;
}

constexpr bool operator!=(PACKET_TYPE lhs, uint32_t rhs) {
  return !(lhs == rhs);
}

constexpr auto SVR_CONNECT = PACKET_TYPE::SVR_CONNECT;
constexpr auto SVR_DISCONNECT = PACKET_TYPE::SVR_DISCONNECT;
constexpr auto SVR_MESSAGE = PACKET_TYPE::SVR_MESSAGE;
constexpr auto SVR_BANNED = PACKET_TYPE::SVR_BANNED;
constexpr auto SVR_SHUTDOWN = PACKET_TYPE::SVR_SHUTDOWN;

constexpr auto CH_JOIN = PACKET_TYPE::CH_JOIN;
constexpr auto CH_LEAVE = PACKET_TYPE::CH_LEAVE;
constexpr auto CH_MESSAGE = PACKET_TYPE::CH_MESSAGE;
constexpr auto CH_UPDATE = PACKET_TYPE::CH_UPDATE;
constexpr auto CH_DELETE = PACKET_TYPE::CH_DELETE;
constexpr auto CH_CREATE = PACKET_TYPE::CH_CREATE;
constexpr auto CH_LIST = PACKET_TYPE::CH_LIST;

constexpr auto CH_INVITE = PACKET_TYPE::CH_INVITE;
constexpr auto CH_KICK = PACKET_TYPE::CH_KICK;
constexpr auto CH_BAN = PACKET_TYPE::CH_BAN;
constexpr auto CH_UNBAN = PACKET_TYPE::CH_UNBAN;

constexpr auto REQUEST_REJECTED = PACKET_TYPE::REQUEST_REJECTED;
constexpr auto PERMISSION_DENIED = PACKET_TYPE::PERMISSION_DENIED;
constexpr auto NOT_FOUND = PACKET_TYPE::NOT_FOUND;
constexpr auto HEARTBEAT = PACKET_TYPE::HEARTBEAT;
constexpr auto ERROR = PACKET_TYPE::ERROR;

struct Response {
  int id{-1};
  int size{-1};
  PACKET_TYPE type;
  std::vector<char> data{};
};

template <typename T>
concept HasDataAndSize = requires(T t) {
  { t.data() } -> std::convertible_to<const void *>;
  { t.size() } -> std::convertible_to<std::size_t>;
};

template <HasDataAndSize T>
inline Response response(const int32_t id, PACKET_TYPE type, const T &data) {
  const auto data_size = static_cast<uint32_t>(data.size() + 10);
  std::vector<char> temporary(data_size + 4);
  std::memcpy(temporary.data() + 0, &data_size, sizeof(data_size));
  std::memcpy(temporary.data() + 4, &id, sizeof(id));
  std::memcpy(temporary.data() + 8, &type, sizeof(type));
  std::memcpy(temporary.data() + 12, data.data(), data.size());
  temporary[data_size] = '\x00';
  temporary[data_size + 1] = '\x00';

  Response packet;
  packet.id = id;
  packet.type = type;
  packet.size = data_size;
  packet.data = temporary;
  return packet;
}

inline Response response(const int32_t id, PACKET_TYPE type) {
  return response(id, type, std::vector<char>());
}

struct Request {
  int id;
  uint32_t type;
  std::vector<uint8_t> payload;

  Request(std::vector<uint8_t> &data) {
    this->id = i32_from_le({data[0], data[1], data[2], data[3]});
    this->type = i32_from_le({data[4], data[5], data[6], data[7]});
    this->payload = std::vector<uint8_t>(&data[8], &data[data.size() - 2]);
  }
};
