#pragma once

#include <memory>

#include "envoy/api/api.h"
#include "envoy/api/v2/core/grpc_service.pb.h"
#include "envoy/common/pure.h"

#include "grpcpp/grpcpp.h"

namespace Envoy {
namespace Grpc {

/**
 * Interface for all Google gRPC credentials factories.
 */
class GoogleGrpcCredentialsFactory {
public:
  virtual ~GoogleGrpcCredentialsFactory() = default;

  /**
   * Get a ChannelCredentials to be used for authentication of a gRPC channel.
   *
   * GoogleGrpcCredentialsFactory should always return a ChannelCredentials. To use CallCredentials,
   * the ChannelCredentials can be created by using a combination of CompositeChannelCredentials and
   * CompositeCallCredentials to combine multiple credentials.
   *
   * @param grpc_service_config contains configuration options
   * @param api reference to the Api object
   * @return std::shared_ptr<grpc::ChannelCredentials> to be used to authenticate a Google gRPC
   * channel.
   */
  virtual std::shared_ptr<grpc::ChannelCredentials>
  getChannelCredentials(const envoy::api::v2::core::GrpcService& grpc_service_config,
                        Api::Api& api) PURE;

  /**
   * @return std::string the identifying name for a particular implementation of
   * a Google gRPC credentials factory.
   */
  virtual std::string name() const PURE;
};

} // namespace Grpc
} // namespace Envoy
