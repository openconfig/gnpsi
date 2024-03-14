#ifndef OPENCONFIG_GNPSI_SERVER_GNPSI_SERVICE_IMPL_H_
#define OPENCONFIG_GNPSI_SERVER_GNPSI_SERVICE_IMPL_H_

#include <cstdint>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/synchronization/mutex.h"
#include "grpcpp/server_context.h"
#include "grpcpp/support/status.h"
#include "proto/gnpsi/gnpsi.grpc.pb.h"
#include "proto/gnpsi/gnpsi.pb.h"

namespace gnpsi {

using ::grpc::ServerContext;
using ::grpc::ServerWriter;
using ::grpc::ServerWriterInterface;
using ::grpc::Status;
using ::grpc::StatusCode;

inline constexpr absl::string_view kIpv4Indicator = "ipv4";
inline constexpr absl::string_view kIpv6Indicator = "ipv6";

struct GnpsiStats {
  GnpsiStats() : datagram_count(0), bytes_sampled(0), error_count(0) {}
  GnpsiStats(absl::string_view ip, int port)
      : collector_ip(ip),
        collector_port(port),
        datagram_count(0),
        bytes_sampled(0),
        error_count(0) {}

  std::string collector_ip;
  int collector_port;
  int datagram_count;
  uint64_t bytes_sampled;
  int error_count;
};

// Interface to gNPSI sender method
class GnpsiSenderInterface {
 public:
  virtual ~GnpsiSenderInterface() = default;
  virtual void SendSamplePacket(
      const std::string& sample_packet,
      SFlowMetadata::Version version = SFlowMetadata::V5) = 0;
  virtual void DrainConnections() = 0;
  virtual std::vector<GnpsiStats> GetStats() = 0;
};

// A connection between a client and gNPSI server. This class is thread-safe.
class GnpsiConnection {
 public:
  explicit GnpsiConnection(ServerContext* context,
                           ServerWriterInterface<Sample>* writer)
      : context_(context), writer_(writer), is_stream_closed_(false) {}

  virtual ~GnpsiConnection() = default;

  virtual std::string GetPeerName() const { return context_->peer(); }

  bool IsContextCancelled() const { return context_->IsCancelled(); }

  bool SendResponse(const ::gnpsi::Sample& response) {
    return writer_->Write(response);
  }

  // Sets is_stream_closed_ to true.
  void CloseStream() ABSL_LOCKS_EXCLUDED(mu_) {
    absl::MutexLock l(&mu_);
    is_stream_closed_ = true;
  }
  // Blocks until connection is closed. This can either be the stream is broken
  // or context is cancelled.
  void WaitUntilClosed() ABSL_LOCKS_EXCLUDED(mu_);

  // Initialize the stats object with the required connection related values.
  // IPV4 Uri = ipv4:a.b.c.d:e will set connection_ip = a.b.c.d and
  // connection_port = e. IPV6 Uri = ipv6:[a:b:c:d::]:e will set connection_ip =
  // a:b:c:d:: and connection_port = e.
  absl::Status InitializeStats();

  // Increment the count of datagram sent to this connection by 1.
  void IncrementDatagramCount() { this->stats_.datagram_count++; }

  // Increment the bytes sampled by size of packet.
  void IncrementBytesSampled(uint64_t packet_size) {
    this->stats_.bytes_sampled += packet_size;
  }

  // Increment the count of write errors for this connection by 1.
  void IncrementWriteErrorCount() { this->stats_.error_count++; }

  // Returns the number of datagrams sent to this connection.
  GnpsiStats GetConnectionStats() { return this->stats_; }

 private:
  ServerContext* context_;
  ServerWriterInterface<::gnpsi::Sample>* writer_;
  // Lock for protecting is_stream_closed_.
  absl::Mutex mu_;
  // When set to true, it means stream is broken.
  bool is_stream_closed_ ABSL_GUARDED_BY(mu_);
  // Maintains the stats for this connections.
  GnpsiStats stats_;
};

// Implementation of gNPSI server.
// 1. Supports Subscribe from clients.
// 2. Exposes an interface for server to send Sample response.
class GnpsiServiceImpl : public ::gnpsi::gNPSI::Service,
                         public GnpsiSenderInterface {
 public:
  explicit GnpsiServiceImpl(int client_max_number)
      : client_max_number_(client_max_number) {}

  // Creates a new GnpsiConnection and adds it into gnpsi_connections_ vector.
  // Returns a FAILED_PRECONDITION error if gnpsi_connections_ size has reached
  // client_max_number_.
  Status Subscribe(ServerContext* context, const Request* request,
                   ServerWriter<Sample>* writer)
      ABSL_LOCKS_EXCLUDED(mu_) override;

  // Sends Sample response to each client.
  void SendSamplePacket(const std::string& sample_packet,
                        SFlowMetadata::Version version = SFlowMetadata::V5)
      ABSL_LOCKS_EXCLUDED(mu_) override;

  // Closes all current conections and blocks any new incoming connections.
  void DrainConnections() ABSL_LOCKS_EXCLUDED(mu_) override;

  // Returns stats per connection collected by the server.
  std::vector<GnpsiStats> GetStats() ABSL_LOCKS_EXCLUDED(mu_) override;

 private:
  int client_max_number_;
  // Lock for protecting member fields.
  absl::Mutex mu_;
  // Adds `connection` to internal vector.
  // Returns true if `connection` is successfully added.
  // Returns false and does nothing if number of alive connections reaches
  // client_max_number_.
  absl::Status AddConnection(GnpsiConnection* connection)
      ABSL_LOCKS_EXCLUDED(mu_);
  // Removes `connection` from gnpsi_connections_ if it exists. Otherwise, does
  // nothing.
  void DropConnection(GnpsiConnection* connection) ABSL_LOCKS_EXCLUDED(mu_);
  // Returns the number of alive connections and marks stale connections as
  // closed.
  int GetAliveConnections() ABSL_EXCLUSIVE_LOCKS_REQUIRED(mu_);
  // Maintains a vector of GnpsiConnection.
  std::vector<GnpsiConnection*> gnpsi_connections_ ABSL_GUARDED_BY(mu_);
  // Indicates whether service drain has been initiated.
  bool service_drained_ ABSL_GUARDED_BY(mu_) = false;
};

}  // namespace gnpsi

#endif  // OPENCONFIG_GNPSI_SERVER_GNPSI_SERVICE_IMPL_H_
