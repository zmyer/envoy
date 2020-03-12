#include "test/integration/integration.h"
#include "test/integration/utility.h"
#include "test/test_common/utility.h"

namespace Envoy {

class DirectResponseIntegrationTest : public testing::TestWithParam<Network::Address::IpVersion>,
                                      public BaseIntegrationTest {
public:
  DirectResponseIntegrationTest() : BaseIntegrationTest(GetParam(), directResponseConfig()) {}

  static std::string directResponseConfig() {
    return ConfigHelper::BASE_CONFIG + R"EOF(
    filter_chains:
      filters:
      - name: direct_response
        typed_config:
          "@type": type.googleapis.com/envoy.extensions.filters.network.direct_response.v3.Config
          response: aGVsbG8sIHdvcmxkIQo=
      )EOF";
  }

  /**
   * Initializer for an individual test.
   */
  void SetUp() override { BaseIntegrationTest::initialize(); }

  /**
   *  Destructor for an individual test.
   */
  void TearDown() override {
    test_server_.reset();
    fake_upstreams_.clear();
  }
};

INSTANTIATE_TEST_SUITE_P(IpVersions, DirectResponseIntegrationTest,
                         testing::ValuesIn(TestEnvironment::getIpVersionsForTest()),
                         TestUtility::ipTestParamsToString);

TEST_P(DirectResponseIntegrationTest, Hello) {
  Buffer::OwnedImpl buffer("hello");
  std::string response;
  RawConnectionDriver connection(
      lookupPort("listener_0"), buffer,
      [&](Network::ClientConnection&, const Buffer::Instance& data) -> void {
        response.append(data.toString());
        connection.close();
      },
      version_);

  connection.run();
  EXPECT_EQ("hello, world!\n", response);
}

} // namespace Envoy
