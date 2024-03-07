#include "server/gnpsi_relay_server.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>

#include "glog/logging.h"
#include "server/gnpsi_service_impl.h"

namespace {
enum ReadError {
  // No Error in the current read
  NoError = 0,
  // Non Fatal Error can retry reading from the socket
  NonFatalError = 1,
  // Fatal Error should stop reading from the socket
  FatalError = 2,
};

int SetUpReadSocket(int port, int addr_family,
                    gnpsi::SocketInterface* socket_provider) {
  int socket_fd = socket_provider->Socket(addr_family, SOCK_DGRAM, IPPROTO_UDP);
  if (socket_fd < 0) {
    LOG(ERROR) << "Socket creation failed: " << strerror(errno);
    return -1;
  }
  struct sockaddr_storage addr;
  socklen_t sock_len = 0;
  memset(&addr, 0, sizeof(addr));
  if (addr_family == AF_INET6) {
    struct sockaddr_in6* address = (struct sockaddr_in6*)&addr;
    sock_len = sizeof(struct sockaddr_in6);
    address->sin6_family = AF_INET6;
    address->sin6_addr = in6addr_loopback;
    address->sin6_port = htons(port);
  } else if (addr_family == AF_INET) {
    struct sockaddr_in* address = (struct sockaddr_in*)&addr;
    sock_len = sizeof(struct sockaddr_in);
    address->sin_family = AF_INET;
    address->sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address->sin_port = htons(port);
  } else {
    LOG(ERROR) << "Invalid address family";
    socket_provider->Close(socket_fd);
    return -1;
  }
  if (socket_provider->Bind(socket_fd, (struct sockaddr*)&addr, sock_len) < 0) {
    LOG(ERROR) << "Failed to Bind socket: " << strerror(errno);
    socket_provider->Close(socket_fd);
    return -1;
  }
  return 0;
  return socket_fd;
}

ReadError ErrorNoToReadError(int error_number) {
  switch (error_number) {
    case EINTR:
    case EAGAIN:
      return ReadError::NonFatalError;
    default:
      return ReadError::FatalError;
  }
}

ReadError Read(int fd, void* buf, size_t count, int& len,
               gnpsi::SocketInterface* socket_provider) {
  ReadError err = ReadError::NoError;
  len = socket_provider->Read(fd, buf, count);
  if (len < 0) {
    err = ErrorNoToReadError(errno);
  }
  return err;
}
}  // namespace

namespace gnpsi {

const int kMaxBufferSize = 4096;

void GnpsiRelayServer::StartRelayAndWait(GnpsiSenderInterface& service) {
  LOG(INFO) << "Setting up socket to read packets.";
  int fd = SetUpReadSocket(udp_port_, addr_family_, socket_provider_.get());
  if (fd < 0) {
    LOG(ERROR) << "Socket creation failed, cannot relay samples.";
    return;
  }
  char buf[kMaxBufferSize];
  LOG(INFO) << "Start reading sample packets.";
  while (true) {
    int len = 0;
    ReadError err = Read(fd, buf, kMaxBufferSize, len, socket_provider_.get());
    if (err == ReadError::FatalError) {
      LOG(ERROR) << "Read from socket failed with a fatal error: "
                 << strerror(errno);
      // Stop reading and relaying samples if error is fatal
      break;
    }
    if (err == ReadError::NonFatalError) {
      VLOG(1) << "Read from socket failed with a non fatal error: "
                   << strerror(errno);
      // Continue relaying samples and ignore current read if error is non fatal
      continue;
    }
    VLOG(1) << "Received sample with size: " << len;
    service.SendSamplePacket(std::string(buf, len));
  }
}
}  // namespace gnpsi
