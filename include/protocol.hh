#pragma once

#include "channel.hh"
#include "client.hh"
#include "managers.hh"
#include "typedef.hh"
#include "utilities.hh"
#include <memory>
#include <sys/socket.h>

namespace Protocol {
struct Context {
  std::shared_ptr<ChannelManager> channels;
  std::shared_ptr<ClientManager> clients;
};

Response handle_request(const std::shared_ptr<Client> s_client,
                        const Request &request);
Response handle_server_connection(const w_client w_client,
                                  const Request &request);
void server_disconnect(const w_client &w_client);

Response channel_join_request(const w_client &w_client, const Request &request);

Response channel_disconnect(const w_client &w_client, const Request &request);

Response channel_message_request(const w_client &w_client,
                                 const Request &request);
} // namespace Protocol
