#include "server/gnpsi_relay_server.h"

#include <asm-generic/errno-base.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <exception>
#include <memory>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "server/mock_gnpsi_service_impl.h"

namespace gnpsi {
namespace {
using testing::_;
using testing::Invoke;
using testing::Return;

const int kUdpPort = 0;
}  // namespace

// This class replaces the normal socket interface for test use
class MockSocket : public SocketInterface {
 public:
  MOCK_METHOD(int, Socket, (int domain, int type, int protocol), (override));
  MOCK_METHOD(ssize_t, Read, (int fd, void* buf, size_t count), (override));
  MOCK_METHOD(int, Bind,
              (int sockfd, const struct sockaddr* addr, socklen_t addrlen),
              (override));
  MOCK_METHOD(int, Close, (int fd), (override));
};

class GnpsiRelayServerTest : public ::testing::Test {
 protected:
  GnpsiRelayServerTest()
      : mock_socket_(new MockSocket), relay_server_(kUdpPort, AF_INET6) {
    relay_server_.set_socket_interface(mock_socket_);
  }

  MockSocket* mock_socket_;
  MockGnpsiServiceImpl gnpsi_service_impl_;
  GnpsiRelayServer relay_server_;
};

TEST_F(GnpsiRelayServerTest, RelaySuccess) {
  EXPECT_CALL(*mock_socket_, Socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP))
      .WillOnce(Return(0));
  EXPECT_CALL(*mock_socket_, Bind(0, _, _)).WillOnce(Return(0));
  EXPECT_CALL(*mock_socket_, Read(0, _, 4096))
      .Times(2)
      .WillOnce(Invoke([&]() {
        errno = 0;
        return 0;
      }))
      .WillOnce(Invoke([&]() {
        errno = EFAULT;
        return -1;
      }));
  // Assert a send call made in case of a successful read
  EXPECT_CALL(gnpsi_service_impl_, SendSamplePacket(_, _)).Times(1);
  relay_server_.StartRelayAndWait(gnpsi_service_impl_);
}

TEST_F(GnpsiRelayServerTest, NoDeathOnNonCriticalReadError) {
  EXPECT_CALL(*mock_socket_, Socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP))
      .WillOnce(Return(0));
  EXPECT_CALL(*mock_socket_, Bind(0, _, _)).WillOnce(Return(0));
  EXPECT_CALL(*mock_socket_, Read(0, _, 4096))
      .Times(2)
      .WillOnce(Invoke([&]() {
        errno = EINTR;
        return -1;
      }))
      .WillOnce(Invoke([&]() {
        errno = EFAULT;
        return -1;
      }));
  // Assert no send call is made in case non fatal error is encountered
  EXPECT_CALL(gnpsi_service_impl_, SendSamplePacket(_, _)).Times(0);
  relay_server_.StartRelayAndWait(gnpsi_service_impl_);
}

TEST_F(GnpsiRelayServerTest, DeathOnSocketCreationError) {
  EXPECT_CALL(*mock_socket_, Socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP))
      .WillOnce(Return(-1));
  // Assert no read calls are made in case of socket setup failure
  EXPECT_CALL(*mock_socket_, Read(0, _, 4096)).Times(0);
  relay_server_.StartRelayAndWait(gnpsi_service_impl_);
}

TEST_F(GnpsiRelayServerTest, DeathOnSocketBindError) {
  EXPECT_CALL(*mock_socket_, Socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP))
      .WillOnce(Return(0));
  EXPECT_CALL(*mock_socket_, Bind(0, _, _)).WillOnce(Return(-1));
  EXPECT_CALL(*mock_socket_, Close(_)).WillOnce(Return(0));
  // Assert no read calls are made in case of socket setup failure
  EXPECT_CALL(*mock_socket_, Read(0, _, 4096)).Times(0);
  relay_server_.StartRelayAndWait(gnpsi_service_impl_);
}

TEST_F(GnpsiRelayServerTest, DeathOnCriticalReadError) {
  EXPECT_CALL(*mock_socket_, Socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP))
      .WillOnce(Return(0));
  EXPECT_CALL(*mock_socket_, Bind(0, _, _)).WillOnce(Return(0));
  EXPECT_CALL(*mock_socket_, Read(0, _, 4096)).Times(1).WillOnce(Invoke([&]() {
    errno = EFAULT;
    return -1;
  }));
  // Assert no send call is made in case fatal error is encountered
  EXPECT_CALL(gnpsi_service_impl_, SendSamplePacket(_, _)).Times(0);
  relay_server_.StartRelayAndWait(gnpsi_service_impl_);
}
}  // namespace gnpsi
