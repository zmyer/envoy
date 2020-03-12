#pragma once

#include <string>

#include "envoy/http/filter.h"

#include "extensions/common/aws/signer.h"
#include "extensions/filters/http/common/pass_through_filter.h"

#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace AwsLambdaFilter {

class Arn {
public:
  Arn(absl::string_view partition, absl::string_view service, absl::string_view region,
      absl::string_view account_id, absl::string_view resource_type,
      absl::string_view function_name)
      : partition_(partition), service_(service), region_(region), account_id_(account_id),
        resource_type_(resource_type), function_name_(function_name) {}

  const std::string& partition() const { return partition_; }
  const std::string& service() const { return service_; }
  const std::string& region() const { return region_; }
  const std::string& accountId() const { return account_id_; }
  const std::string& resourceType() const { return resource_type_; }
  const std::string& functionName() const { return function_name_; }

private:
  std::string partition_;
  std::string service_;
  std::string region_;
  std::string account_id_;
  std::string resource_type_;
  std::string function_name_; // resource_id
};

/**
 * Parses the input string into a structured ARN.
 *
 * The format is expected to be as such:
 * arn:partition:service:region:account-id:resource-type:resource-id
 *
 * Lambda ARN Example:
 * arn:aws:lambda:us-west-2:987654321:function:hello_envoy
 */
absl::optional<Arn> parseArn(absl::string_view arn);

class FilterSettings : public Router::RouteSpecificFilterConfig {
public:
  FilterSettings(const std::string& arn, bool payload_passthrough)
      : arn_(arn), payload_passthrough_(payload_passthrough) {}

  const std::string& arn() const { return arn_; }
  bool payloadPassthrough() const { return payload_passthrough_; }

private:
  std::string arn_;
  bool payload_passthrough_;
};

class Filter : public Http::PassThroughFilter, Logger::Loggable<Logger::Id::filter> {

public:
  Filter(const FilterSettings& config,
         const std::shared_ptr<Extensions::Common::Aws::Signer>& sigv4_signer);

  Http::FilterHeadersStatus decodeHeaders(Http::RequestHeaderMap& headers,
                                          bool end_stream) override;
  Http::FilterDataStatus decodeData(Buffer::Instance& data, bool end_stream) override;

private:
  /**
   * Calculates the route specific Lambda ARN if any.
   */
  absl::optional<Arn> calculateRouteArn();
  const FilterSettings settings_;
  Http::RequestHeaderMap* headers_ = nullptr;
  std::shared_ptr<Extensions::Common::Aws::Signer> sigv4_signer_;
  absl::optional<Arn> arn_;
  bool skip_ = false;
};

} // namespace AwsLambdaFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
