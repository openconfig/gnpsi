#include "server/gnpsi_service_impl.h"

#include <memory>
#include <string>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "glog/logging.h"
#include "absl/status/status.h"
#include "absl/strings/match.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/strings/strip.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "proto/gnpsi/gnpsi.pb.h"

namespace gnpsi {

absl::Status GnpsiConnection::InitializeStats() {
  std::string uri = this->GetPeerName();
  LOG(INFO) << "uri: " << uri;
  absl::string_view ip;
  int port;
  if (absl::StartsWith(uri, kIpv4Indicator)) {
    absl::string_view path =
        absl::StripPrefix(uri, absl::StrCat(kIpv4Indicator, ":"));
    if (int colon = path.find(':'); colon != absl::string_view::npos) {
      ip = path.substr(0, colon);
      if (!absl::SimpleAtoi(path.substr(colon + 1), &port)) {
        return absl::InternalError(
            "Error retrieving port information from uri string");
      }
      this->stats_ = GnpsiStats(ip, port);
      return absl::OkStatus();
    }
    return absl::NotFoundError("Port not found in uri string");
  }
  if (absl::StartsWith(uri, kIpv6Indicator)) {
    absl::string_view path =
        absl::StripPrefix(uri, absl::StrCat(kIpv6Indicator, ":%5B"));
    if (int closing_bracket = path.find("%5D");
        closing_bracket != absl::string_view::npos) {
      ip = path.substr(0, closing_bracket);
      if (!absl::SimpleAtoi(path.substr(closing_bracket + 4), &port)) {
        return absl::InternalError(
            "Error retrieving port information from uri string");
      }
      this->stats_ = GnpsiStats(ip, port);
      return absl::OkStatus();
    }
    return absl::NotFoundError("Port not found in uri string");
  }
  return absl::InvalidArgumentError("The passed URI format is not supported");
}

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
  if (absl::Status status = connection->InitializeStats(); !status.ok()) {
    LOG(ERROR) << "Error while creating stats object for peer - "
               << status.message();
  }
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
      connection->IncrementDatagramCount();
      connection->IncrementBytesSampled(sample_packet.size());
    } else {
      // If it fails to send response to any client, close this connection.
      LOG(ERROR) << "Failed to send sample packet to "
                 << connection->GetPeerName() << ".";
      connection->IncrementWriteErrorCount();
      connection->CloseStream();
    }
  }
}

std::vector<GnpsiStats> GnpsiServiceImpl::GetStats() {
  absl::MutexLock l(&mu_);
  std::vector<GnpsiStats> stats;
  for (auto it = gnpsi_connections_.begin(), end = gnpsi_connections_.end();
       it != end; it++) {
    stats.push_back((*it)->GetConnectionStats());
  }
  return stats;
}
}  // namespace gnpsi
