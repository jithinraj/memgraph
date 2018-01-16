#pragma once

#include "communication/messaging/distributed.hpp"
#include "communication/rpc/rpc.hpp"
#include "transactions/engine_single_node.hpp"

namespace tx {

/** Distributed master transaction engine. Has complete engine functionality and
 * exposes an RPC server to be used by distributed Workers. */
class MasterEngine : public SingleNodeEngine {
 public:
  /**
   * @param wal - Optional. If present, the Engine will write tx
   * Begin/Commit/Abort atomically (while under lock).
   */
  MasterEngine(communication::messaging::System &system,
               durability::WriteAheadLog *wal = nullptr);

 private:
  communication::rpc::Server rpc_server_;
};
}  // namespace tx