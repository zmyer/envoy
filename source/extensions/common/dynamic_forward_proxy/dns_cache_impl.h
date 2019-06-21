#pragma once

#include "envoy/network/dns.h"
#include "envoy/thread_local/thread_local.h"

#include "common/common/cleanup.h"

#include "extensions/common/dynamic_forward_proxy/dns_cache.h"

#include "absl/container/flat_hash_map.h"

namespace Envoy {
namespace Extensions {
namespace Common {
namespace DynamicForwardProxy {

class DnsCacheImpl : public DnsCache, Logger::Loggable<Logger::Id::forward_proxy> {
public:
  DnsCacheImpl(Event::Dispatcher& main_thread_dispatcher, ThreadLocal::SlotAllocator& tls,
               const envoy::config::common::dynamic_forward_proxy::v2alpha::DnsCacheConfig& config);
  ~DnsCacheImpl();

  // DnsCache
  LoadDnsCacheHandlePtr loadDnsCache(absl::string_view host, uint16_t default_port,
                                     LoadDnsCacheCallbacks& callbacks) override;
  AddUpdateCallbacksHandlePtr addUpdateCallbacks(UpdateCallbacks& callbacks) override;

private:
  using TlsHostMap = absl::flat_hash_map<std::string, DnsHostInfoSharedPtr>;
  using TlsHostMapSharedPtr = std::shared_ptr<TlsHostMap>;

  struct LoadDnsCacheHandleImpl : public LoadDnsCacheHandle,
                                  RaiiListElement<LoadDnsCacheHandleImpl*> {
    LoadDnsCacheHandleImpl(std::list<LoadDnsCacheHandleImpl*>& parent, absl::string_view host,
                           LoadDnsCacheCallbacks& callbacks)
        : RaiiListElement<LoadDnsCacheHandleImpl*>(parent, this), host_(host),
          callbacks_(callbacks) {}

    const std::string host_;
    LoadDnsCacheCallbacks& callbacks_;
  };

  // Per-thread DNS cache info including the currently known hosts as well as any pending callbacks.
  struct ThreadLocalHostInfo : public ThreadLocal::ThreadLocalObject {
    ~ThreadLocalHostInfo();
    void updateHostMap(const TlsHostMapSharedPtr& new_host_map);

    TlsHostMapSharedPtr host_map_;
    std::list<LoadDnsCacheHandleImpl*> pending_resolutions_;
  };

  struct DnsHostInfoImpl : public DnsHostInfo {
    DnsHostInfoImpl(TimeSource& time_source) : time_source_(time_source) { touch(); }

    // DnsHostInfo
    Network::Address::InstanceConstSharedPtr address() override { return address_; }
    void touch() override { last_used_time_ = time_source_.monotonicTime().time_since_epoch(); }

    TimeSource& time_source_;
    Network::Address::InstanceConstSharedPtr address_;
    // Using std::chrono::steady_clock::duration is required for compilation within an atomic vs.
    // using MonotonicTime.
    std::atomic<std::chrono::steady_clock::duration> last_used_time_;
  };

  using DnsHostInfoImplSharedPtr = std::shared_ptr<DnsHostInfoImpl>;

  // Primary host information that accounts for TTL, re-resolution, etc.
  struct PrimaryHostInfo {
    PrimaryHostInfo(DnsCacheImpl& parent, absl::string_view host_to_resolve, uint16_t port,
                    const Event::TimerCb& timer_cb)
        : host_to_resolve_(host_to_resolve), port_(port),
          refresh_timer_(parent.main_thread_dispatcher_.createTimer(timer_cb)) {}

    const std::string host_to_resolve_;
    const uint16_t port_;
    const Event::TimerPtr refresh_timer_;
    DnsHostInfoImplSharedPtr host_info_;
    Network::ActiveDnsQuery* active_query_{};
  };

  using PrimaryHostInfoPtr = std::unique_ptr<PrimaryHostInfo>;

  struct AddUpdateCallbacksHandleImpl : public AddUpdateCallbacksHandle,
                                        RaiiListElement<AddUpdateCallbacksHandleImpl*> {
    AddUpdateCallbacksHandleImpl(std::list<AddUpdateCallbacksHandleImpl*>& parent,
                                 UpdateCallbacks& callbacks)
        : RaiiListElement<AddUpdateCallbacksHandleImpl*>(parent, this), callbacks_(callbacks) {}

    UpdateCallbacks& callbacks_;
  };

  void startCacheLoad(const std::string& host, uint16_t default_port);
  void startResolve(const std::string& host, PrimaryHostInfo& host_info);
  void finishResolve(const std::string& host,
                     const std::list<Network::Address::InstanceConstSharedPtr>& address_list);
  void runAddUpdateCallbacks(const std::string& host, const DnsHostInfoSharedPtr& host_info);
  void runRemoveCallbacks(const std::string& host);
  void updateTlsHostsMap();
  void onReResolve(const std::string& host);

  Event::Dispatcher& main_thread_dispatcher_;
  const Network::DnsLookupFamily dns_lookup_family_;
  const Network::DnsResolverSharedPtr resolver_;
  const ThreadLocal::SlotPtr tls_slot_;
  std::list<AddUpdateCallbacksHandleImpl*> update_callbacks_;
  absl::flat_hash_map<std::string, PrimaryHostInfoPtr> primary_hosts_;
  const std::chrono::milliseconds refresh_interval_;
  const std::chrono::milliseconds host_ttl_;
};

} // namespace DynamicForwardProxy
} // namespace Common
} // namespace Extensions
} // namespace Envoy
