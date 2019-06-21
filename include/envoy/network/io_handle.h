#pragma once

#include "envoy/api/io_error.h"
#include "envoy/common/pure.h"

namespace Envoy {
namespace Buffer {
struct RawSlice;
} // namespace Buffer

namespace Network {
namespace Address {
class Instance;
} // namespace Address

/**
 * IoHandle: an abstract interface for all I/O operations
 */
class IoHandle {
public:
  virtual ~IoHandle() = default;

  /**
   * Return data associated with IoHandle.
   *
   * TODO(danzh) move it to IoSocketHandle after replacing the calls to it with
   * calls to IoHandle API's everywhere.
   */
  virtual int fd() const PURE;

  /**
   * Clean up IoHandle resources
   */
  virtual Api::IoCallUint64Result close() PURE;

  /**
   * Return true if close() hasn't been called.
   */
  virtual bool isOpen() const PURE;

  /**
   * Read data into given slices.
   * @param max_length supplies the maximum length to read.
   * @param slices points to the output location.
   * @param num_slice indicates the number of slices |slices| contains.
   * @return a Api::IoCallUint64Result with err_ = an Api::IoError instance or
   * err_ = nullptr and rc_ = the bytes read for success.
   */
  virtual Api::IoCallUint64Result readv(uint64_t max_length, Buffer::RawSlice* slices,
                                        uint64_t num_slice) PURE;

  /**
   * Write the data in slices out.
   * @param slices points to the location of data to be written.
   * @param num_slice indicates number of slices |slices| contains.
   * @return a Api::IoCallUint64Result with err_ = an Api::IoError instance or
   * err_ = nullptr and rc_ = the bytes written for success.
   */
  virtual Api::IoCallUint64Result writev(const Buffer::RawSlice* slices, uint64_t num_slice) PURE;

  /**
   * Send buffer to address.
   * @param slice points to the location of the data to be sent.
   * @param to_address is the destination address.
   * @return a Api::IoCallUint64Result with err_ = an Api::IoError instance or
   * err_ = nullptr and rc_ = the bytes written for success.
   */
  virtual Api::IoCallUint64Result sendto(const Buffer::RawSlice& slice, int flags,
                                         const Address::Instance& address) PURE;
  /**
   * Send a message to the address.
   * @param slices points to the location of data to be sent.
   * @param num_slice indicates number of slices |slices| contains.
   * @param address is the destination address.
   * @return a Api::IoCallUint64Result with err_ = an Api::IoError instance or
   * err_ = nullptr and rc_ = the bytes written for success.
   */
  virtual Api::IoCallUint64Result sendmsg(const Buffer::RawSlice* slices, uint64_t num_slice,
                                          int flags, const Address::Instance& address) PURE;
};

using IoHandlePtr = std::unique_ptr<IoHandle>;

} // namespace Network
} // namespace Envoy
