#include "configurations.hh"
#include "server.hpp"
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

/* Args
 * --channels=0
 * --clients=0
 * --threads=0
 * --port=0000
 */
int main(int argc, char *argv[]) {
  // global configuration class;
  auto &configuration = ServerConfiguration::instance();

  // if args are given, swap the minimum value with the given value.
  if (argc > 1) {
    try {
      for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg.rfind("--channels=", 0) == 0) {
          auto substr = arg.substr(11);
          configuration.set_max_channels(std::stoi(substr));
          continue;
        } else if (arg.rfind("--clients=", 0) == 0) {
          auto substr = arg.substr(10);
          configuration.set_max_clients(std::stoi(substr));
          continue;
        } else if (arg.rfind("--threads=", 0) == 0) {
          auto substr = arg.substr(10);
          configuration.set_pool_size(std::stoi(substr));
          continue;
        } else if (arg.rfind("--port=", 0) == 0) {
          auto substr = arg.substr(7);
          configuration.set_port(std::stoi(substr));
        }
      }
    } catch (const std::invalid_argument &e) {
      std::cout << "Invalid argument: " << e.what() << std::endl;
    } catch (const std::out_of_range &e) {
    }
  }
  std::shared_ptr<Server> server = std::make_shared<Server>();
  server->listen();
  return 0;
}
