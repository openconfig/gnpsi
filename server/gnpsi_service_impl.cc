#include "server/gnpsi_service_impl.h"

#include <memory>
#include <string>

#include "absl/base/thread_annotations.h"
#include "glog/logging.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "proto/gnpsi/gnpsi.pb.h"

namespace gnpsi {

void GnpsiConnection::WaitUntilClosed() {
  absl::MutexLock l(&mu_);
  auto stream_disconnected = [this]() ABSL_EXCLUSIVE_LOCKS_REQUIRED(mu_) {
    return is_stream_closed_;
  };
  mu_.Await(absl::Condition(&stream_disconnected));
}

Status GnpsiServiceImpl::Subscribe(ServerContext* context,
                                   const Request* request,
                                   ServerWriter<Sample>* writer) {
  if (context == nullptr) {
    LOG(ERROR) << "StreamSflowSample context is a nullptr.";
    return Status(StatusCode::INVALID_ARGUMENT, "Context cannot be nullptr.");
  }
  auto connection = std::make_unique<GnpsiConnection>(context, writer);
  absl::Status status = AddConnection(connection.get());
  if (!status.ok()) {
    return Status(StatusCode(status.code()), std::string(status.message()));
  }
  // Send Initial Metadata after adding connection indicating that the client
  // should be receiving any new samples from this point on.
  writer->SendInitialMetadata();
  // Blocks until it is closed by client and removes it from internal vector.
  connection->WaitUntilClosed();
  DropConnection(connection.get());
  return Status::OK;
}

int GnpsiServiceImpl::GetAliveConnections() {
  // Check if any connection is stale.
  int alive_connections = gnpsi_connections_.size();
  for (GnpsiConnection* connection : gnpsi_connections_) {
    if (connection->IsContextCancelled()) {
      alive_connections--;
      connection->CloseStream();
    }
  }
  return alive_connections;
}

absl::Status GnpsiServiceImpl::AddConnection(GnpsiConnection* connection) {
  absl::MutexLock l(&mu_);
  if (service_drained_) {
    LOG(ERROR) << "Cannot add connections since the service has been drained.";
    return absl::FailedPreconditionError(
        "No more conections can be added after the service is drained.");
  }
  if (GetAliveConnections() >= client_max_number_) {
    LOG(ERROR) << "GnpsiService only supports at most " << client_max_number_
               << " clients.";
    return absl::FailedPreconditionError(
        absl::StrCat("Number of clients exceeds maximum number. "
                     "GnpsiService supports at most ",
                     client_max_number_, " clients."));
  }
  LOG(INFO) << "Add " << connection->GetPeerName() << " to client list.";
  gnpsi_connections_.push_back(connection);
  return absl::OkStatus();
}

// Iterate through the connections vector and remove `connection` if it exists.
void GnpsiServiceImpl::DropConnection(GnpsiConnection* connection) {
  absl::MutexLock l(&mu_);
  for (auto iter = gnpsi_connections_.begin(); iter != gnpsi_connections_.end();
       ++iter) {
    if (*iter == connection) {
      LOG(INFO) << "Dropping gNPSI connection to " << connection->GetPeerName();
      gnpsi_connections_.erase(iter);
      break;
    }
  }
}

void GnpsiServiceImpl::DrainConnections() {
  absl::MutexLock lock(&mu_);
  service_drained_ = true;
  // Close all active connections.
  for (auto it = gnpsi_connections_.begin(), end = gnpsi_connections_.end();
       it != end; it++) {
    auto connection = *it;
    connection->CloseStream();
  }
}

void GnpsiServiceImpl::SendSamplePacket(
    const std::string& sample_packet, ::gnpsi::SFlowMetadata::Version version) {
  absl::MutexLock l(&mu_);
  Sample response;
  response.set_packet(sample_packet);
  response.set_timestamp(absl::ToUnixNanos(absl::Now()));
  response.mutable_sflow_metadata()->set_version(version);
  for (auto it = gnpsi_connections_.begin(), end = gnpsi_connections_.end();
       it != end; it++) {
    auto connection = *it;
    if (!connection->IsContextCancelled() &&
        connection->SendResponse(response)) {
      VLOG(1) << "Successfully sent sample packet to "
              << connection->GetPeerName() << ".";
    } else {
      // If it fails to send response to any client, close this connection.
      LOG(ERROR) << "Failed to send sample packet to "
                 << connection->GetPeerName() << ".";
      connection->CloseStream();
    }
  }
}

}  // namespace gnpsi
