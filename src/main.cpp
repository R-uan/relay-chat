#include "configurations.hh"
#include "server.hpp"
#include "spdlog/common.h"
#include "spdlog/logger.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/spdlog.h"
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

void setup_logger(bool debug_mode) {
  using namespace spdlog;

  auto fsink = std::make_shared<sinks::basic_file_sink_mt>("chat.log", true);
  auto csink = std::make_shared<sinks::stdout_color_sink_mt>();

  // Set patterns first
  csink->set_pattern("[%d-%m-%Y %T] [%^%l%$]: %v");
  fsink->set_pattern("[%d-%m-%Y %T] [%^%l%$]: %v");

  // Set sink levels based on debug mode
  if (debug_mode) {
    csink->set_level(level::debug); // Console shows debug and above
    fsink->set_level(level::off);   // File logging off in debug mode
  } else {
    csink->set_level(level::info);  // Console shows info and above
    fsink->set_level(level::trace); // File logs everything (trace and above)
  }

  // OR simpler approach:
  std::vector<spdlog::sink_ptr> sinks = {csink, fsink};
  auto logger = std::make_shared<spdlog::logger>("relay_chat", sinks.begin(),
                                                 sinks.end());

  // Set the GLOBAL level first - this is the primary filter
  if (debug_mode) {
    spdlog::set_level(level::debug); // Global minimum level
    logger->set_level(level::debug); // Logger-specific level
  } else {
    spdlog::set_level(level::info); // Global minimum level
    logger->set_level(level::info); // Logger-specific level
  }

  // Set as default logger
  spdlog::set_default_logger(logger);

  if (debug_mode) {
    spdlog::debug("Debug mode ENABLED");
  }
}

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
        if (arg.rfind("--debug") == 0) {
          configuration.set_debug();
        } else if (arg.rfind("--channels=", 0) == 0) {
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

  setup_logger(configuration.debugging());
  std::shared_ptr<Server> server = std::make_shared<Server>();
  server->listen();
  return 0;
}
