#include "utilities.hh"
#include <gtest/gtest.h>
#include <vector>

TEST(REQ_RES_CONSTRUCTOR, REQUEST_CONSTRUCTOR) {
  std::vector<uint8_t> bytes = {0x01, 0x00, 0x00, 0x00, 0x16, 0x00,
                                0x00, 0x00, 'b',  'n',  'u',  'y'};
  Request request(bytes);
  EXPECT_EQ(request.id, 1);
  EXPECT_EQ(request.type, 22);
}
