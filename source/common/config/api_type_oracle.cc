#include "common/config/api_type_oracle.h"

#include "udpa/annotations/versioning.pb.h"

namespace Envoy {
namespace Config {

const Protobuf::Descriptor*
ApiTypeOracle::getEarlierVersionDescriptor(const std::string& message_type) {
  // Determine if there is an earlier API version for message_type.
  const Protobuf::Descriptor* desc =
      Protobuf::DescriptorPool::generated_pool()->FindMessageTypeByName(std::string{message_type});
  if (desc == nullptr) {
    return nullptr;
  }
  if (desc->options().HasExtension(udpa::annotations::versioning)) {
    const Protobuf::Descriptor* earlier_desc =
        Protobuf::DescriptorPool::generated_pool()->FindMessageTypeByName(
            desc->options().GetExtension(udpa::annotations::versioning).previous_message_type());
    return earlier_desc;
  }

  return nullptr;
}

} // namespace Config
} // namespace Envoy
