#include <experimental/filesystem>
#include <iostream>

#include "gflags/gflags.h"

#include "dbms/dbms.hpp"
#include "query/engine.hpp"

#include "communication/bolt/v1/session.hpp"
#include "communication/server.hpp"

#include "io/network/network_endpoint.hpp"
#include "io/network/network_error.hpp"
#include "io/network/socket.hpp"

#include "logging/default.hpp"
#include "logging/streams/stdout.hpp"

#include "utils/signals/handler.hpp"
#include "utils/stacktrace/log.hpp"
#include "utils/terminate_handler.hpp"

namespace fs = std::experimental::filesystem;
using endpoint_t = io::network::NetworkEndpoint;
using socket_t = io::network::Socket;
using session_t = communication::bolt::Session<socket_t>;
using result_stream_t =
    communication::bolt::ResultStream<communication::bolt::Encoder<
        communication::bolt::ChunkedEncoderBuffer<socket_t>>>;
using bolt_server_t =
    communication::Server<session_t, result_stream_t, socket_t>;

Logger logger;

DEFINE_string(interface, "0.0.0.0", "Default interface on which to listen.");
DEFINE_string(port, "7687", "Default port on which to listen.");
DEFINE_int32(num_workers, std::thread::hardware_concurrency(),
             "Number of workers");

void throw_and_stacktace(std::string message) {
  Stacktrace stacktrace;
  logger.info(stacktrace.dump());
}

// Load flags in this order, the last one has the highest priority:
// 1) /etc/memgraph/config
// 2) ~/.memgraph/config
// 3) env - MEMGRAPH_CONFIG
// 4) command line flags

void load_config(int &argc, char **&argv) {
  std::vector<fs::path> configs = {fs::path("/etc/memgraph/config")};
  if (getenv("HOME") != nullptr)
    configs.emplace_back(fs::path(getenv("HOME")) /
                         fs::path(".memgraph/config"));
  if (getenv("MEMGRAPH_CONFIG") != nullptr)
    configs.emplace_back(fs::path(getenv("MEMGRAPH_CONFIG")));

  std::vector<std::string> flagfile_arguments;
  for (const auto &config : configs)
    if (fs::exists(config)) {
      flagfile_arguments.emplace_back(
          std::string("--flagfile=" + config.generic_string()));
    }

  int custom_argc = static_cast<int>(flagfile_arguments.size()) + 1;
  char **custom_argv = new char *[custom_argc];

  custom_argv[0] = strdup(std::string("memgraph").c_str());
  for (int i = 0; i < (int)flagfile_arguments.size(); ++i) {
    custom_argv[i + 1] = strdup(flagfile_arguments[i].c_str());
  }

  // setup flags from config flags
  gflags::ParseCommandLineFlags(&custom_argc, &custom_argv, false);

  // unconsumed arguments have to be freed to avoid memory leak since they are
  // strdup-ed.
  for (int i = 0; i < custom_argc; ++i) free(custom_argv[i]);
  delete[] custom_argv;

  // setup flags from command line
  gflags::ParseCommandLineFlags(&argc, &argv, true);
}

int main(int argc, char **argv) {
  fs::current_path(fs::path(argv[0]).parent_path());
  load_config(argc, argv);

// Logging init.
#ifdef SYNC_LOGGER
  logging::init_sync();
#else
  logging::init_async();
#endif
  logging::log->pipe(std::make_unique<Stdout>());

  // Get logger.
  logger = logging::log->logger("Main");
  logger.info("{}", logging::log->type());

  // Unhandled exception handler init.
  std::set_terminate(&terminate_handler);

  // Signal handling init.
  SignalHandler::register_handler(Signal::SegmentationFault, []() {
    log_stacktrace("SegmentationFault signal raised");
    std::exit(EXIT_FAILURE);
  });
  SignalHandler::register_handler(Signal::Abort, []() {
    log_stacktrace("Abort signal raised");
    std::exit(EXIT_FAILURE);
  });

  // Initialize endpoint.
  endpoint_t endpoint;
  try {
    endpoint = endpoint_t(FLAGS_interface, FLAGS_port);
  } catch (io::network::NetworkEndpointException &e) {
    logger.error("{}", e.what());
    std::exit(EXIT_FAILURE);
  }

  // Initialize socket.
  socket_t socket;
  if (!socket.Bind(endpoint)) {
    logger.error("Cannot bind to socket on {} at {}", FLAGS_interface,
                 FLAGS_port);
    std::exit(EXIT_FAILURE);
  }
  if (!socket.SetNonBlocking()) {
    logger.error("Cannot set socket to non blocking!");
    std::exit(EXIT_FAILURE);
  }
  if (!socket.Listen(1024)) {
    logger.error("Cannot listen on socket!");
    std::exit(EXIT_FAILURE);
  }

  logger.info("Listening on {} at {}", FLAGS_interface, FLAGS_port);

  Dbms dbms;
  QueryEngine<result_stream_t> query_engine;

  // Initialize server.
  bolt_server_t server(std::move(socket), dbms, query_engine);

  // register SIGTERM handler
  SignalHandler::register_handler(Signal::Terminate, [&server]() {
    server.Shutdown();
    std::exit(EXIT_SUCCESS);
  });

  // register SIGINT handler
  SignalHandler::register_handler(Signal::Interupt, [&server]() {
    server.Shutdown();
    std::exit(EXIT_FAILURE);
  });

  // Start worker threads.
  logger.info("Starting {} workers", FLAGS_num_workers);
  server.Start(FLAGS_num_workers);

  logger.info("Shutting down...");
  return EXIT_SUCCESS;
}
