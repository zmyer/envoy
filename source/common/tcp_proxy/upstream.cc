#include "common/tcp_proxy/upstream.h"

namespace Envoy {
namespace TcpProxy {

TcpUpstream::TcpUpstream(Tcp::ConnectionPool::ConnectionDataPtr&& data,
                         Tcp::ConnectionPool::UpstreamCallbacks& upstream_callbacks)
    : upstream_conn_data_(std::move(data)) {
  Network::ClientConnection& connection = upstream_conn_data_->connection();
  connection.enableHalfClose(true);
  upstream_conn_data_->addUpstreamCallbacks(upstream_callbacks);
}

bool TcpUpstream::readDisable(bool disable) {
  if (upstream_conn_data_ == nullptr ||
      upstream_conn_data_->connection().state() != Network::Connection::State::Open) {
    // Because we flush write downstream, we can have a case where upstream has already disconnected
    // and we are waiting to flush. If we had a watermark event during this time we should no
    // longer touch the upstream connection.
    return false;
  }

  upstream_conn_data_->connection().readDisable(disable);
  return true;
}

void TcpUpstream::encodeData(Buffer::Instance& data, bool end_stream) {
  upstream_conn_data_->connection().write(data, end_stream);
}

void TcpUpstream::addBytesSentCallback(Network::Connection::BytesSentCb cb) {
  upstream_conn_data_->connection().addBytesSentCallback(cb);
}

Tcp::ConnectionPool::ConnectionData*
TcpUpstream::onDownstreamEvent(Network::ConnectionEvent event) {
  if (event == Network::ConnectionEvent::RemoteClose) {
    // The close call may result in this object being deleted. Latch the
    // connection locally so it can be returned for potential draining.
    auto* conn_data = upstream_conn_data_.release();
    conn_data->connection().close(Network::ConnectionCloseType::FlushWrite);
    return conn_data;
  } else if (event == Network::ConnectionEvent::LocalClose) {
    upstream_conn_data_->connection().close(Network::ConnectionCloseType::NoFlush);
  }
  return nullptr;
}

} // namespace TcpProxy
} // namespace Envoy
