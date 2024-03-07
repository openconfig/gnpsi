#ifndef OPENCONFIG_GNPSI_SERVER_GNPSI_RELAY_SERVER_H_
#define OPENCONFIG_GNPSI_SERVER_GNPSI_RELAY_SERVER_H_

#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <memory>

#include "server/gnpsi_service_impl.h"

namespace gnpsi {
class SocketInterface {
 public:
  virtual ~SocketInterface() {}
  virtual int Socket(int domain, int type, int protocol) = 0;
  virtual ssize_t Read(int fd, void* buf, size_t count) = 0;
  virtual int Bind(int sockfd, const struct sockaddr* addr,
                   socklen_t addrlen) = 0;
  virtual int Close(int fd) = 0;
};

class SocketProvider : public SocketInterface {
 public:
  // Sets up a socket using the socket system call
  int Socket(int domain, int type, int protocol) override {
    return socket(domain, type, protocol);
  }
  // Reads bytes using the read system call
  ssize_t Read(int fd, void* buf, size_t count) override {
    return read(fd, buf, count);
  }
  // Binds a socket to a address using the bind system call
  int Bind(int sockfd, const struct sockaddr* addr,
           socklen_t addrlen) override {
    return bind(sockfd, addr, addrlen);
  }
  // Closes a socket using the close system call
  int Close(int fd) override { return close(fd); }
};

class GnpsiRelayServer {
 public:
  GnpsiRelayServer(int udp_port, int addr_family)
      : udp_port_(udp_port),
        addr_family_(addr_family),
        socket_provider_(new SocketProvider) {}

  // Start Relaying Samples by reading from the udp port. This is a blocking
  // call and will keep on reading samples until a critical error is encountered
  void StartRelayAndWait(GnpsiSenderInterface& service);

  // Mutator
  void set_socket_interface(SocketInterface* new_interface) {
    socket_provider_.reset(new_interface);
  }

 private:
  int udp_port_;
  int addr_family_;
  // The socket_ provides access calls to setup and read from sockets.  Unit
  // tests can replace the normally constructed interface with a mock interface.
  std::unique_ptr<SocketInterface> socket_provider_;
};

}  // namespace gnpsi
#endif  // OPENCONFIG_GNPSI_SERVER_GNPSI_RELAY_SERVER_H_
