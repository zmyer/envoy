#include <cstddef>

#include "envoy/config/bootstrap/v3/bootstrap.pb.h"
#include "envoy/extensions/filters/network/http_connection_manager/v3/http_connection_manager.pb.h"
#include "envoy/extensions/transport_sockets/tls/v3/cert.pb.h"

#include "test/config/utility.h"
#include "test/integration/http_integration.h"
#include "test/test_common/utility.h"

#pragma GCC diagnostic push
// QUICHE allows unused parameters.
#pragma GCC diagnostic ignored "-Wunused-parameter"
// QUICHE uses offsetof().
#pragma GCC diagnostic ignored "-Winvalid-offsetof"

#include "quiche/quic/core/http/quic_client_push_promise_index.h"
#include "quiche/quic/core/quic_utils.h"
#include "quiche/quic/test_tools/quic_test_utils.h"

#pragma GCC diagnostic pop

#include "extensions/quic_listeners/quiche/envoy_quic_client_session.h"
#include "extensions/quic_listeners/quiche/envoy_quic_client_connection.h"
#include "extensions/quic_listeners/quiche/envoy_quic_fake_proof_verifier.h"
#include "extensions/quic_listeners/quiche/envoy_quic_connection_helper.h"
#include "extensions/quic_listeners/quiche/envoy_quic_alarm_factory.h"
#include "extensions/quic_listeners/quiche/envoy_quic_packet_writer.h"
#include "extensions/quic_listeners/quiche/envoy_quic_utils.h"

namespace Envoy {
namespace Quic {

class CodecClientCallbacksForTest : public Http::CodecClientCallbacks {
public:
  void onStreamDestroy() override {}

  void onStreamReset(Http::StreamResetReason reason) override {
    last_stream_reset_reason_ = reason;
  }

  Http::StreamResetReason last_stream_reset_reason_{Http::StreamResetReason::LocalReset};
};

class QuicHttpIntegrationTest : public testing::TestWithParam<Network::Address::IpVersion>,
                                public HttpIntegrationTest {
public:
  QuicHttpIntegrationTest()
      : HttpIntegrationTest(Http::CodecClient::Type::HTTP3, GetParam(),
                            ConfigHelper::QUIC_HTTP_PROXY_CONFIG),
        supported_versions_(quic::CurrentSupportedVersions()),
        crypto_config_(std::make_unique<EnvoyQuicFakeProofVerifier>()), conn_helper_(*dispatcher_),
        alarm_factory_(*dispatcher_, *conn_helper_.GetClock()) {}

  Network::ClientConnectionPtr makeClientConnection(uint32_t port) override {
    server_addr_ = Network::Utility::resolveUrl(
        fmt::format("udp://{}:{}", Network::Test::getLoopbackAddressUrlString(version_), port));
    Network::Address::InstanceConstSharedPtr local_addr =
        Network::Test::getCanonicalLoopbackAddress(version_);
    // Initiate a QUIC connection with the highest supported version. If not
    // supported by server, this connection will fail.
    // TODO(danzh) Implement retry upon version mismatch and modify test frame work to specify a
    // different version set on server side to test that.
    auto connection = std::make_unique<EnvoyQuicClientConnection>(
        getNextServerDesignatedConnectionId(), server_addr_, conn_helper_, alarm_factory_,
        quic::ParsedQuicVersionVector{supported_versions_[0]}, local_addr, *dispatcher_, nullptr);
    quic_connection_ = connection.get();
    auto session = std::make_unique<EnvoyQuicClientSession>(
        quic_config_, supported_versions_, std::move(connection), server_id_, &crypto_config_,
        &push_promise_index_, *dispatcher_, 0);
    session->Initialize();
    return session;
  }

  // This call may fail because of INVALID_VERSION, because QUIC connection doesn't support
  // in-connection version negotiation.
  // TODO(#8479) Propagate INVALID_VERSION error to caller and let caller to use server advertised
  // version list to create a new connection with mutually supported version and make client codec
  // again.
  IntegrationCodecClientPtr makeRawHttpConnection(Network::ClientConnectionPtr&& conn) override {
    IntegrationCodecClientPtr codec = HttpIntegrationTest::makeRawHttpConnection(std::move(conn));
    if (codec->disconnected()) {
      // Connection may get closed during version negotiation or handshake.
      ENVOY_LOG(error, "Fail to connect to server with error: {}",
                codec->connection()->transportFailureReason());
    } else {
      codec->setCodecClientCallbacks(client_codec_callback_);
    }
    return codec;
  }

  quic::QuicConnectionId getNextServerDesignatedConnectionId() {
    quic::QuicCryptoClientConfig::CachedState* cached = crypto_config_.LookupOrCreate(server_id_);
    // If the cached state indicates that we should use a server-designated
    // connection ID, then return that connection ID.
    quic::QuicConnectionId conn_id = cached->has_server_designated_connection_id()
                                         ? cached->GetNextServerDesignatedConnectionId()
                                         : quic::EmptyQuicConnectionId();
    return conn_id.IsEmpty() ? quic::QuicUtils::CreateRandomConnectionId() : conn_id;
  }

  void initialize() override {
    config_helper_.addConfigModifier([this](envoy::config::bootstrap::v3::Bootstrap& bootstrap) {
      envoy::extensions::transport_sockets::tls::v3::DownstreamTlsContext tls_context;
      ConfigHelper::initializeTls({}, *tls_context.mutable_common_tls_context());
      auto* filter_chain =
          bootstrap.mutable_static_resources()->mutable_listeners(0)->mutable_filter_chains(0);
      auto* transport_socket = filter_chain->mutable_transport_socket();
      transport_socket->mutable_typed_config()->PackFrom(tls_context);

      bootstrap.mutable_static_resources()->mutable_listeners(0)->set_reuse_port(set_reuse_port_);
    });
    config_helper_.addConfigModifier(
        [](envoy::extensions::filters::network::http_connection_manager::v3::HttpConnectionManager&
               hcm) {
          EXPECT_EQ(hcm.codec_type(), envoy::extensions::filters::network::http_connection_manager::
                                          v3::HttpConnectionManager::HTTP3);
        });

    HttpIntegrationTest::initialize();
    registerTestServerPorts({"http"});
  }

protected:
  quic::QuicConfig quic_config_;
  quic::QuicServerId server_id_{"example.com", 443, false};
  quic::QuicClientPushPromiseIndex push_promise_index_;
  quic::ParsedQuicVersionVector supported_versions_;
  quic::QuicCryptoClientConfig crypto_config_;
  EnvoyQuicConnectionHelper conn_helper_;
  EnvoyQuicAlarmFactory alarm_factory_;
  CodecClientCallbacksForTest client_codec_callback_;
  Network::Address::InstanceConstSharedPtr server_addr_;
  EnvoyQuicClientConnection* quic_connection_{nullptr};
  bool set_reuse_port_{false};
};

INSTANTIATE_TEST_SUITE_P(IpVersions, QuicHttpIntegrationTest,
                         testing::ValuesIn(TestEnvironment::getIpVersionsForTest()),
                         TestUtility::ipTestParamsToString);

TEST_P(QuicHttpIntegrationTest, GetRequestAndEmptyResponse) {
  testRouterHeaderOnlyRequestAndResponse();
}

TEST_P(QuicHttpIntegrationTest, GetRequestAndResponseWithBody) {
  initialize();
  sendRequestAndVerifyResponse(default_request_headers_, /*request_size=*/0,
                               default_response_headers_, /*response_size=*/1024,
                               /*backend_index*/ 0);
}

TEST_P(QuicHttpIntegrationTest, PostRequestAndResponseWithBody) {
  testRouterRequestAndResponseWithBody(1024, 512, false);
}

TEST_P(QuicHttpIntegrationTest, PostRequestWithBigHeadersAndResponseWithBody) {
  testRouterRequestAndResponseWithBody(1024, 512, true);
}

TEST_P(QuicHttpIntegrationTest, RouterUpstreamDisconnectBeforeRequestcomplete) {
  testRouterUpstreamDisconnectBeforeRequestComplete();
}

TEST_P(QuicHttpIntegrationTest, RouterUpstreamDisconnectBeforeResponseComplete) {
  testRouterUpstreamDisconnectBeforeResponseComplete();
  EXPECT_EQ(Http::StreamResetReason::RemoteReset, client_codec_callback_.last_stream_reset_reason_);
}

TEST_P(QuicHttpIntegrationTest, RouterDownstreamDisconnectBeforeRequestComplete) {
  testRouterDownstreamDisconnectBeforeRequestComplete();
}

TEST_P(QuicHttpIntegrationTest, RouterDownstreamDisconnectBeforeResponseComplete) {
  testRouterDownstreamDisconnectBeforeResponseComplete();
}

TEST_P(QuicHttpIntegrationTest, RouterUpstreamResponseBeforeRequestComplete) {
  testRouterUpstreamResponseBeforeRequestComplete();
}

TEST_P(QuicHttpIntegrationTest, Retry) { testRetry(); }

TEST_P(QuicHttpIntegrationTest, UpstreamReadDisabledOnGiantResponseBody) {
  config_helper_.setBufferLimits(/*upstream_buffer_limit=*/1024, /*downstream_buffer_limit=*/1024);
  testRouterRequestAndResponseWithBody(/*request_size=*/512, /*response_size=*/1024 * 1024, false);
}

TEST_P(QuicHttpIntegrationTest, DownstreamReadDisabledOnGiantPost) {
  config_helper_.setBufferLimits(/*upstream_buffer_limit=*/1024, /*downstream_buffer_limit=*/1024);
  testRouterRequestAndResponseWithBody(/*request_size=*/1024 * 1024, /*response_size=*/1024, false);
}

// Tests that a connection idle times out after 1s and starts delayed close.
TEST_P(QuicHttpIntegrationTest, TestDelayedConnectionTeardownTimeoutTrigger) {
  config_helper_.addFilter("{ name: envoy.filters.http.dynamo, typed_config: { \"@type\": "
                           "type.googleapis.com/google.protobuf.Empty } }");
  config_helper_.setBufferLimits(1024, 1024);
  config_helper_.addConfigModifier(
      [](envoy::extensions::filters::network::http_connection_manager::v3::HttpConnectionManager&
             hcm) {
        // 200ms.
        hcm.mutable_delayed_close_timeout()->set_nanos(200000000);
        hcm.mutable_drain_timeout()->set_seconds(1);
        hcm.mutable_common_http_protocol_options()->mutable_idle_timeout()->set_seconds(1);
      });

  initialize();

  fake_upstreams_[0]->set_allow_unexpected_disconnects(true);

  codec_client_ = makeHttpConnection(lookupPort("http"));

  auto encoder_decoder =
      codec_client_->startRequest(Http::TestRequestHeaderMapImpl{{":method", "POST"},
                                                                 {":path", "/test/long/url"},
                                                                 {":scheme", "http"},
                                                                 {":authority", "host"}});
  request_encoder_ = &encoder_decoder.first;
  auto response = std::move(encoder_decoder.second);

  codec_client_->sendData(*request_encoder_, 1024 * 65, false);

  response->waitForEndStream();
  // The delayed close timeout should trigger since client is not closing the connection.
  EXPECT_TRUE(codec_client_->waitForDisconnect(std::chrono::milliseconds(5000)));
  EXPECT_EQ(codec_client_->last_connection_event(), Network::ConnectionEvent::RemoteClose);
  EXPECT_EQ(test_server_->counter("http.config_test.downstream_cx_delayed_close_timeout")->value(),
            1);
}

TEST_P(QuicHttpIntegrationTest, MultipleQuicListenersWithBPF) {
#if defined(SO_ATTACH_REUSEPORT_CBPF) && defined(__linux__)
  concurrency_ = 8;
  set_reuse_port_ = true;
  initialize();
  std::vector<IntegrationCodecClientPtr> codec_clients;
  quic::QuicCryptoClientConfig::CachedState* cached = crypto_config_.LookupOrCreate(server_id_);
  for (size_t i = 1; i <= concurrency_; ++i) {
    // The BPF filter looks at the 1st word of connection id in the packet
    // header. And currently all QUIC versions support 8 bytes connection id. So
    // create connections with the first 4 bytes of connection id different from each
    // other so they should be evenly distributed.
    cached->add_server_designated_connection_id(quic::test::TestConnectionId(i << 32));
    codec_clients.push_back(makeHttpConnection(lookupPort("http")));
  }
  if (GetParam() == Network::Address::IpVersion::v4) {
    test_server_->waitForCounterEq("listener.0.0.0.0_0.downstream_cx_total", 8u);
  } else {
    test_server_->waitForCounterEq("listener.[__]_0.downstream_cx_total", 8u);
  }
  for (size_t i = 0; i < concurrency_; ++i) {
    if (GetParam() == Network::Address::IpVersion::v4) {
      test_server_->waitForGaugeEq(
          fmt::format("listener.0.0.0.0_0.worker_{}.downstream_cx_active", i), 1u);
      test_server_->waitForCounterEq(
          fmt::format("listener.0.0.0.0_0.worker_{}.downstream_cx_total", i), 1u);
    } else {
      test_server_->waitForGaugeEq(fmt::format("listener.[__]_0.worker_{}.downstream_cx_active", i),
                                   1u);
      test_server_->waitForCounterEq(
          fmt::format("listener.[__]_0.worker_{}.downstream_cx_total", i), 1u);
    }
  }
  for (size_t i = 0; i < concurrency_; ++i) {
    codec_clients[i]->close();
  }
#endif
}

#ifndef __APPLE__
TEST_P(QuicHttpIntegrationTest, MultipleQuicListenersNoBPF) {
  concurrency_ = 8;
  set_reuse_port_ = true;
  initialize();
#ifdef SO_ATTACH_REUSEPORT_CBPF
#define SO_ATTACH_REUSEPORT_CBPF_TMP SO_ATTACH_REUSEPORT_CBPF
#undef SO_ATTACH_REUSEPORT_CBPF
#endif
  std::vector<IntegrationCodecClientPtr> codec_clients;
  quic::QuicCryptoClientConfig::CachedState* cached = crypto_config_.LookupOrCreate(server_id_);
  for (size_t i = 1; i <= concurrency_; ++i) {
    // The BPF filter looks at the 1st byte of connection id in the packet
    // header. And currently all QUIC versions support 8 bytes connection id. So
    // create connections with the first 4 bytes of connection id different from each
    // other so they should be evenly distributed.
    cached->add_server_designated_connection_id(quic::test::TestConnectionId(i << 32));
    codec_clients.push_back(makeHttpConnection(lookupPort("http")));
  }
  if (GetParam() == Network::Address::IpVersion::v4) {
    test_server_->waitForCounterEq("listener.0.0.0.0_0.downstream_cx_total", 8u);
  } else {
    test_server_->waitForCounterEq("listener.[__]_0.downstream_cx_total", 8u);
  }
  // Even without BPF support, these connections should more or less distributed
  // across different workers.
  for (size_t i = 0; i < concurrency_; ++i) {
    if (GetParam() == Network::Address::IpVersion::v4) {
      EXPECT_LT(
          test_server_->gauge(fmt::format("listener.0.0.0.0_0.worker_{}.downstream_cx_active", i))
              ->value(),
          8u);
      EXPECT_LT(
          test_server_->counter(fmt::format("listener.0.0.0.0_0.worker_{}.downstream_cx_total", i))
              ->value(),
          8u);
    } else {
      EXPECT_LT(
          test_server_->gauge(fmt::format("listener.[__]_0.worker_{}.downstream_cx_active", i))
              ->value(),
          8u);
      EXPECT_LT(
          test_server_->counter(fmt::format("listener.[__]_0.worker_{}.downstream_cx_total", i))
              ->value(),
          8u);
    }
  }
  for (size_t i = 0; i < concurrency_; ++i) {
    codec_clients[i]->close();
  }
#ifdef SO_ATTACH_REUSEPORT_CBPF_TMP
#define SO_ATTACH_REUSEPORT_CBPF SO_ATTACH_REUSEPORT_CBPF_TMP
#endif
}
#endif

#if defined(SO_ATTACH_REUSEPORT_CBPF) && defined(__linux__)
TEST_P(QuicHttpIntegrationTest, ConnectionMigration) {
  concurrency_ = 2;
  set_reuse_port_ = true;
  initialize();
  uint32_t old_port = lookupPort("http");
  codec_client_ = makeHttpConnection(old_port);
  auto encoder_decoder =
      codec_client_->startRequest(Http::TestRequestHeaderMapImpl{{":method", "POST"},
                                                                 {":path", "/test/long/url"},
                                                                 {":scheme", "http"},
                                                                 {":authority", "host"}});
  request_encoder_ = &encoder_decoder.first;
  auto response = std::move(encoder_decoder.second);

  codec_client_->sendData(*request_encoder_, 1024u, false);

  // Change to a new port by switching socket, and connection should still continue.
  Network::Address::InstanceConstSharedPtr local_addr =
      Network::Test::getCanonicalLoopbackAddress(version_);
  quic_connection_->switchConnectionSocket(
      createConnectionSocket(server_addr_, local_addr, nullptr));
  EXPECT_NE(old_port, local_addr->ip()->port());
  // Send the rest data.
  codec_client_->sendData(*request_encoder_, 1024u, true);
  waitForNextUpstreamRequest(0, TestUtility::DefaultTimeout);
  // Send response headers, and end_stream if there is no response body.
  const Http::TestResponseHeaderMapImpl response_headers{{":status", "200"}};
  size_t response_size{5u};
  upstream_request_->encodeHeaders(response_headers, false);
  upstream_request_->encodeData(response_size, true);
  response->waitForEndStream();
  verifyResponse(std::move(response), "200", response_headers, std::string(response_size, 'a'));

  EXPECT_TRUE(upstream_request_->complete());
  EXPECT_EQ(1024u * 2, upstream_request_->bodyLength());
  cleanupUpstreamAndDownstream();
}
#endif

} // namespace Quic
} // namespace Envoy
