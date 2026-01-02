#include "utilities.hh"
#include <cstdint>
#include <cstring>

int i32_from_le(const std::vector<uint8_t> &bytes) {
  return static_cast<int>(bytes[0] | bytes[1] << 8 | bytes[2] << 16 |
                          bytes[3] << 24);
}

std::vector<std::vector<uint8_t>> split_newline(std::vector<uint8_t> &data) {
  std::vector<std::vector<uint8_t>> lines;
  std::vector<uint8_t> current;

  for (auto byte : data) {
    if (byte == '\n') {
      lines.push_back(current);
      current.clear();
    } else if (byte == 0x00) {
      continue;
    } else {
      current.push_back(byte);
    }
  }

  if (!current.empty())
    lines.push_back(current);

  return lines;
}
