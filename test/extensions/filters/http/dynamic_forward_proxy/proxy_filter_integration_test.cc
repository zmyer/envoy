#include "test/integration/http_integration.h"

namespace Envoy {
namespace {

class ProxyFilterIntegrationTest : public testing::TestWithParam<Network::Address::IpVersion>,
                                   public HttpIntegrationTest {
public:
  ProxyFilterIntegrationTest() : HttpIntegrationTest(Http::CodecClient::Type::HTTP1, GetParam()) {}

  static std::string ipVersionToDnsFamily(Network::Address::IpVersion version) {
    switch (version) {
    case Network::Address::IpVersion::v4:
      return "V4_ONLY";
    case Network::Address::IpVersion::v6:
      return "V6_ONLY";
    }

    // This seems to be needed on the coverage build for some reason.
    NOT_REACHED_GCOVR_EXCL_LINE;
  }

  void SetUp() override {
    setUpstreamProtocol(FakeHttpConnection::Type::HTTP1);

    const std::string filter = fmt::format(R"EOF(
name: envoy.filters.http.dynamic_forward_proxy
config:
  dns_cache_config:
    name: foo
    dns_lookup_family: {}
)EOF",
                                           ipVersionToDnsFamily(GetParam()));
    config_helper_.addFilter(filter);

    config_helper_.addConfigModifier([](envoy::config::bootstrap::v2::Bootstrap& bootstrap) {
      auto* cluster_0 = bootstrap.mutable_static_resources()->mutable_clusters(0);
      cluster_0->clear_hosts();
      cluster_0->set_lb_policy(envoy::api::v2::Cluster::CLUSTER_PROVIDED);

      const std::string cluster_type_config = fmt::format(R"EOF(
name: envoy.clusters.dynamic_forward_proxy
typed_config:
  "@type": type.googleapis.com/envoy.config.cluster.dynamic_forward_proxy.v2alpha.ClusterConfig
  dns_cache_config:
    name: foo
    dns_lookup_family: {}
)EOF",
                                                          ipVersionToDnsFamily(GetParam()));

      TestUtility::loadFromYaml(cluster_type_config, *cluster_0->mutable_cluster_type());
    });

    HttpIntegrationTest::initialize();
  }
};

INSTANTIATE_TEST_SUITE_P(IpVersions, ProxyFilterIntegrationTest,
                         testing::ValuesIn(TestEnvironment::getIpVersionsForTest()),
                         TestUtility::ipTestParamsToString);

// A basic test where we pause a request to lookup localhost, and then do another request which
// should hit the TLS cache.
TEST_P(ProxyFilterIntegrationTest, RequestWithBody) {
  codec_client_ = makeHttpConnection(lookupPort("http"));
  Http::TestHeaderMapImpl request_headers{
      {":method", "POST"},
      {":path", "/test/long/url"},
      {":scheme", "http"},
      {":authority",
       fmt::format("localhost:{}", fake_upstreams_[0]->localAddress()->ip()->port())}};

  auto response =
      sendRequestAndWaitForResponse(request_headers, 1024, default_response_headers_, 1024);
  checkSimpleRequestSuccess(1024, 1024, response.get());

  // Now send another request. This should hit the DNS cache.
  // TODO(mattklein123): Verify this with stats once stats are added.
  response = sendRequestAndWaitForResponse(request_headers, 512, default_response_headers_, 512);
  checkSimpleRequestSuccess(512, 512, response.get());
}

// TODO(mattklein123): Add a test for host expiration. We can do this both with simulated time
// and by checking stats.

} // namespace
} // namespace Envoy
