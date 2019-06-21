#pragma once

#include <set>
#include <string>
#include <vector>

#include "envoy/api/v2/cds.pb.h"
#include "envoy/common/pure.h"

#include "common/protobuf/protobuf.h"

namespace Envoy {
namespace Upstream {

/**
 * Type of load balancing to perform.
 */
enum class LoadBalancerType {
  RoundRobin,
  LeastRequest,
  Random,
  RingHash,
  OriginalDst,
  Maglev,
  ClusterProvided
};

struct SubsetSelector {
  std::set<std::string> selector_keys_;
  envoy::api::v2::Cluster::LbSubsetConfig::LbSubsetSelector::LbSubsetSelectorFallbackPolicy
      fallback_policy_;
};

using SubsetSelectorPtr = std::shared_ptr<SubsetSelector>;

/**
 * Load Balancer subset configuration.
 */
class LoadBalancerSubsetInfo {
public:
  virtual ~LoadBalancerSubsetInfo() = default;

  /**
   * @return bool true if load balancer subsets are configured.
   */
  virtual bool isEnabled() const PURE;

  /**
   * @return LbSubsetFallbackPolicy the fallback policy used when
   * route metadata does not match any subset.
   */
  virtual envoy::api::v2::Cluster::LbSubsetConfig::LbSubsetFallbackPolicy
  fallbackPolicy() const PURE;

  /**
   * @return ProtobufWkt::Struct the struct describing the metadata for a
   *         host to be included in the default subset.
   */
  virtual const ProtobufWkt::Struct& defaultSubset() const PURE;

  /*
   * @return const std:vector<std:set<std::string>>& a vector of
   * sorted keys used to define load balancer subsets.
   */
  virtual const std::vector<SubsetSelectorPtr>& subsetSelectors() const PURE;

  /*
   * @return bool whether routing to subsets should take locality weights into account.
   */
  virtual bool localityWeightAware() const PURE;

  /*
   * @return bool whether the locality weights should be scaled to compensate for the
   * fraction of hosts removed from the original host set.
   */
  virtual bool scaleLocalityWeight() const PURE;

  /*
   * @return bool whether to attempt to select a host from the entire cluster if host
   * selection from the fallback subset fails.
   */
  virtual bool panicModeAny() const PURE;
};

} // namespace Upstream
} // namespace Envoy
