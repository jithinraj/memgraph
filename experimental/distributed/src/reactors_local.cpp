#include "reactors_local.hpp"

void EventStream::Subscription::unsubscribe() const {
  event_queue_.RemoveCb(*this);
}

thread_local Reactor* current_reactor_ = nullptr;

std::string Connector::LocalChannel::ReactorName() {
  return reactor_name_;
}

std::string Connector::LocalChannel::Name() {
  return connector_name_;
}

void Connector::LocalEventStream::Close() {
  current_reactor_->CloseConnector(connector_name_);
}

std::pair<EventStream*, std::shared_ptr<Channel>> Reactor::Open(const std::string &connector_name) {
  std::unique_lock<std::mutex> lock(*mutex_);
  // TODO: Improve the check that the channel name does not exist in the
  // system.
  if (connectors_.count(connector_name) != 0) {
    throw std::runtime_error("Connector with name " + connector_name
        + "already exists");
  }
  auto it = connectors_.emplace(connector_name,
    std::make_shared<Connector>(Connector::Params{name_, connector_name, mutex_, cvar_})).first;
  it->second->self_ptr_ = it->second;
  return make_pair(&it->second->stream_, it->second->LockedOpenChannel());
}

std::pair<EventStream*, std::shared_ptr<Channel>> Reactor::Open() {
  std::unique_lock<std::mutex> lock(*mutex_);
  do {
    std::string connector_name = "stream-" + std::to_string(connector_name_counter_++);
    if (connectors_.count(connector_name) == 0) {
      // Connector &queue = connectors_[connector_name];
      auto it = connectors_.emplace(connector_name,
        std::make_shared<Connector>(Connector::Params{name_, connector_name, mutex_, cvar_})).first;
      it->second->self_ptr_ = it->second;
      return make_pair(&it->second->stream_, it->second->LockedOpenChannel());
    }
  } while (true);
}

const std::shared_ptr<Channel> Reactor::FindChannel(
    const std::string &channel_name) {
  std::unique_lock<std::mutex> lock(*mutex_);
  auto it_connector = connectors_.find(channel_name);
  if (it_connector == connectors_.end()) return nullptr;
  return it_connector->second->LockedOpenChannel();
}

void Reactor::CloseConnector(const std::string &s) {
  std::unique_lock<std::mutex> lock(*mutex_);
  auto it = connectors_.find(s);
  assert(it != connectors_.end());
  connectors_.erase(it);
}

void Reactor::CloseAllConnectors() {
  std::unique_lock<std::mutex> lock(*mutex_);
  connectors_.clear();
}

void Reactor::RunEventLoop() {
  bool exit_event_loop = false;

  while (true) {
    // Find (or wait) for the next Message.
    MsgAndCbInfo msg_and_cb;
    {
      std::unique_lock<std::mutex> lock(*mutex_);

      while (true) {

        // Exit the loop if there are no more Connectors.
        if (connectors_.empty()) {
          exit_event_loop = true;
          break;
        }

        // Not fair because was taken earlier, talk to lion.
        msg_and_cb = LockedGetPendingMessages();
        if (msg_and_cb.first != nullptr) break;

        cvar_->wait(lock);
      }

      if (exit_event_loop) break;
    }

    for (auto& cbAndSub : msg_and_cb.second) {
      auto& cb = cbAndSub.first;
      const Message& msg = *msg_and_cb.first;
      cb(msg, cbAndSub.second);
    }
  }
}

/**
 * Checks if there is any nonempty EventStream.
 */
auto Reactor::LockedGetPendingMessages() -> MsgAndCbInfo {
  // return type after because the scope Reactor:: is not searched before the name
  for (auto& connectors_key_value : connectors_) {
    Connector& event_queue = *connectors_key_value.second;
    auto msg_ptr = event_queue.LockedPop();
    if (msg_ptr == nullptr) continue;
    std::type_index tidx = msg_ptr->GetTypeIndex();

    std::vector<std::pair<EventStream::Callback, EventStream::Subscription> > cb_info;
    auto msg_type_cb_iter = event_queue.callbacks_.find(tidx);
    if (msg_type_cb_iter != event_queue.callbacks_.end()) { // There is a callback for this type.
      for (auto& tidx_cb_key_value : msg_type_cb_iter->second) {
        uint64_t uid = tidx_cb_key_value.first;
        EventStream::Callback cb = tidx_cb_key_value.second;
        cb_info.emplace_back(cb, EventStream::Subscription(event_queue, tidx, uid));
      }
    }

    return MsgAndCbInfo(std::move(msg_ptr), std::move(cb_info));
  }

  return MsgAndCbInfo(nullptr, {});
}