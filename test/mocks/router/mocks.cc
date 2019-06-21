#include "mocks.h"

#include <chrono>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

using testing::_;
using testing::NiceMock;
using testing::Return;
using testing::ReturnPointee;
using testing::ReturnRef;
using testing::SaveArg;

namespace Envoy {
namespace Router {

MockDirectResponseEntry::MockDirectResponseEntry() {}
MockDirectResponseEntry::~MockDirectResponseEntry() {}

MockRetryState::MockRetryState() {}

void MockRetryState::expectHeadersRetry() {
  EXPECT_CALL(*this, shouldRetryHeaders(_, _))
      .WillOnce(DoAll(SaveArg<1>(&callback_), Return(RetryStatus::Yes)));
}

void MockRetryState::expectHedgedPerTryTimeoutRetry() {
  EXPECT_CALL(*this, shouldHedgeRetryPerTryTimeout(_))
      .WillOnce(DoAll(SaveArg<0>(&callback_), Return(RetryStatus::Yes)));
}

void MockRetryState::expectResetRetry() {
  EXPECT_CALL(*this, shouldRetryReset(_, _))
      .WillOnce(DoAll(SaveArg<1>(&callback_), Return(RetryStatus::Yes)));
}

MockRetryState::~MockRetryState() {}

MockRateLimitPolicyEntry::MockRateLimitPolicyEntry() {
  ON_CALL(*this, disableKey()).WillByDefault(ReturnRef(disable_key_));
}

MockRateLimitPolicyEntry::~MockRateLimitPolicyEntry() {}

MockRateLimitPolicy::MockRateLimitPolicy() {
  ON_CALL(*this, getApplicableRateLimit(_)).WillByDefault(ReturnRef(rate_limit_policy_entry_));
  ON_CALL(*this, empty()).WillByDefault(Return(true));
}

MockRateLimitPolicy::~MockRateLimitPolicy() {}

MockShadowWriter::MockShadowWriter() {}
MockShadowWriter::~MockShadowWriter() {}

MockVirtualHost::MockVirtualHost() {
  ON_CALL(*this, name()).WillByDefault(ReturnRef(name_));
  ON_CALL(*this, rateLimitPolicy()).WillByDefault(ReturnRef(rate_limit_policy_));
}

MockVirtualHost::~MockVirtualHost() {}

MockHashPolicy::MockHashPolicy() {}
MockHashPolicy::~MockHashPolicy() {}

MockMetadataMatchCriteria::MockMetadataMatchCriteria() {}
MockMetadataMatchCriteria::~MockMetadataMatchCriteria() {}

MockPathMatchCriterion::MockPathMatchCriterion() {
  ON_CALL(*this, matchType()).WillByDefault(ReturnPointee(&type_));
  ON_CALL(*this, matcher()).WillByDefault(ReturnPointee(&matcher_));
}

MockPathMatchCriterion::~MockPathMatchCriterion() {}

MockRouteEntry::MockRouteEntry() {
  ON_CALL(*this, clusterName()).WillByDefault(ReturnRef(cluster_name_));
  ON_CALL(*this, opaqueConfig()).WillByDefault(ReturnRef(opaque_config_));
  ON_CALL(*this, rateLimitPolicy()).WillByDefault(ReturnRef(rate_limit_policy_));
  ON_CALL(*this, retryPolicy()).WillByDefault(ReturnRef(retry_policy_));
  ON_CALL(*this, shadowPolicy()).WillByDefault(ReturnRef(shadow_policy_));
  ON_CALL(*this, timeout()).WillByDefault(Return(std::chrono::milliseconds(10)));
  ON_CALL(*this, virtualCluster(_)).WillByDefault(Return(&virtual_cluster_));
  ON_CALL(*this, virtualHost()).WillByDefault(ReturnRef(virtual_host_));
  ON_CALL(*this, includeVirtualHostRateLimits()).WillByDefault(Return(true));
  ON_CALL(*this, pathMatchCriterion()).WillByDefault(ReturnRef(path_match_criterion_));
  ON_CALL(*this, metadata()).WillByDefault(ReturnRef(metadata_));
  ON_CALL(*this, upgradeMap()).WillByDefault(ReturnRef(upgrade_map_));
  ON_CALL(*this, hedgePolicy()).WillByDefault(ReturnRef(hedge_policy_));
  ON_CALL(*this, routeName()).WillByDefault(ReturnRef(route_name_));
}

MockRouteEntry::~MockRouteEntry() {}

MockConfig::MockConfig() : route_(new NiceMock<MockRoute>()) {
  ON_CALL(*this, route(_, _)).WillByDefault(Return(route_));
  ON_CALL(*this, internalOnlyHeaders()).WillByDefault(ReturnRef(internal_only_headers_));
  ON_CALL(*this, name()).WillByDefault(ReturnRef(name_));
  ON_CALL(*this, usesVhds()).WillByDefault(Return(false));
}

MockConfig::~MockConfig() {}

MockDecorator::MockDecorator() {
  ON_CALL(*this, getOperation()).WillByDefault(ReturnRef(operation_));
}
MockDecorator::~MockDecorator() {}

MockRouteTracing::MockRouteTracing() {}
MockRouteTracing::~MockRouteTracing() {}

MockRoute::MockRoute() {
  ON_CALL(*this, routeEntry()).WillByDefault(Return(&route_entry_));
  ON_CALL(*this, decorator()).WillByDefault(Return(&decorator_));
  ON_CALL(*this, tracingConfig()).WillByDefault(Return(nullptr));
}
MockRoute::~MockRoute() {}

MockRouteConfigProviderManager::MockRouteConfigProviderManager() {}
MockRouteConfigProviderManager::~MockRouteConfigProviderManager() {}

MockScopedConfig::MockScopedConfig() {}
MockScopedConfig::~MockScopedConfig() {}

} // namespace Router
} // namespace Envoy
