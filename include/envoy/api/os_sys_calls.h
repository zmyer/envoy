#pragma once

#ifndef WIN32
#include <sys/ioctl.h>
#include <sys/mman.h>   // for mode_t
#include <sys/socket.h> // for sockaddr
#include <sys/stat.h>
#include <sys/uio.h> // for iovec

#endif

#include <memory>
#include <string>

#include "envoy/api/os_sys_calls_common.h"
#include "envoy/common/pure.h"
#include "envoy/common/platform.h"

namespace Envoy {
namespace Api {

class OsSysCalls {
public:
  virtual ~OsSysCalls() = default;

  /**
   * @see bind (man 2 bind)
   */
  virtual SysCallIntResult bind(int sockfd, const sockaddr* addr, socklen_t addrlen) PURE;

  /**
   * @see ioctl (man 2 ioctl)
   */
  virtual SysCallIntResult ioctl(int sockfd, unsigned long int request, void* argp) PURE;

  /**
   * @see writev (man 2 writev)
   */
  virtual SysCallSizeResult writev(int fd, const iovec* iovec, int num_iovec) PURE;

  /**
   * @see readv (man 2 readv)
   */
  virtual SysCallSizeResult readv(int fd, const iovec* iovec, int num_iovec) PURE;

  /**
   * @see recv (man 2 recv)
   */
  virtual SysCallSizeResult recv(int socket, void* buffer, size_t length, int flags) PURE;

  /**
   * @see recv (man 2 recvfrom)
   */
  virtual SysCallSizeResult recvfrom(int sockfd, void* buffer, size_t length, int flags,
                                     struct sockaddr* addr, socklen_t* addrlen) PURE;
  /**
   * Release all resources allocated for fd.
   * @return zero on success, -1 returned otherwise.
   */
  virtual SysCallIntResult close(int fd) PURE;

  /**
   * @see man 2 ftruncate
   */
  virtual SysCallIntResult ftruncate(int fd, off_t length) PURE;

  /**
   * @see man 2 mmap
   */
  virtual SysCallPtrResult mmap(void* addr, size_t length, int prot, int flags, int fd,
                                off_t offset) PURE;

  /**
   * @see man 2 stat
   */
  virtual SysCallIntResult stat(const char* pathname, struct stat* buf) PURE;

  /**
   * @see man 2 setsockopt
   */
  virtual SysCallIntResult setsockopt(int sockfd, int level, int optname, const void* optval,
                                      socklen_t optlen) PURE;

  /**
   * @see man 2 getsockopt
   */
  virtual SysCallIntResult getsockopt(int sockfd, int level, int optname, void* optval,
                                      socklen_t* optlen) PURE;

  /**
   * @see man 2 socket
   */
  virtual SysCallIntResult socket(int domain, int type, int protocol) PURE;

  /**
   * @see man 2 sendto
   */
  virtual SysCallSizeResult sendto(int fd, const void* buffer, size_t size, int flags,
                                   const sockaddr* addr, socklen_t addrlen) PURE;

  /**
   * @see man 2 sendmsg
   */
  virtual SysCallSizeResult sendmsg(int fd, const msghdr* message, int flags) PURE;

  /**
   * @see man 2 getsockname
   */
  virtual SysCallIntResult getsockname(int sockfd, sockaddr* addr, socklen_t* addrlen) PURE;
};

using OsSysCallsPtr = std::unique_ptr<OsSysCalls>;

} // namespace Api
} // namespace Envoy
