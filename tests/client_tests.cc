#include "client.hh"
#include "utilities.hh"
#include "gmock/gmock.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <memory>
#include <string>

class MockClient : public Client {
public:
  MOCK_METHOD(bool, send_packet, (const Response), (override));
};

class ClientTest : public ::testing::Test {
protected:
  std::shared_ptr<Client> client;
  void SetUp() override { client = std::make_shared<Client>(1, 1); }
};

TEST_F(ClientTest, ChangeUsername_FormatsCorrectly) {
  std::string username = "alice";
  auto result = client->change_username(username);

  EXPECT_EQ(result, "alice@1");
  EXPECT_EQ(client->username, "alice@1");
}
