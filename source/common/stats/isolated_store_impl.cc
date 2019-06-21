#include "common/stats/isolated_store_impl.h"

#include <string.h>

#include <algorithm>
#include <string>

#include "common/common/utility.h"
#include "common/stats/fake_symbol_table_impl.h"
#include "common/stats/histogram_impl.h"
#include "common/stats/scope_prefixer.h"
#include "common/stats/utility.h"

namespace Envoy {
namespace Stats {

IsolatedStoreImpl::IsolatedStoreImpl()
    : IsolatedStoreImpl(std::make_unique<FakeSymbolTableImpl>()) {}

IsolatedStoreImpl::IsolatedStoreImpl(std::unique_ptr<SymbolTable>&& symbol_table)
    : IsolatedStoreImpl(*symbol_table) {
  symbol_table_storage_ = std::move(symbol_table);
}

IsolatedStoreImpl::IsolatedStoreImpl(SymbolTable& symbol_table)
    : StoreImpl(symbol_table), alloc_(symbol_table),
      counters_([this](StatName name) -> CounterSharedPtr {
        return alloc_.makeCounter(name, alloc_.symbolTable().toString(name), std::vector<Tag>());
      }),
      gauges_([this](StatName name, Gauge::ImportMode import_mode) -> GaugeSharedPtr {
        return alloc_.makeGauge(name, alloc_.symbolTable().toString(name), std::vector<Tag>(),
                                import_mode);
      }),
      histograms_([this](StatName name) -> HistogramSharedPtr {
        return std::make_shared<HistogramImpl>(name, *this, alloc_.symbolTable().toString(name),
                                               std::vector<Tag>());
      }),
      null_gauge_(symbol_table) {}

ScopePtr IsolatedStoreImpl::createScope(const std::string& name) {
  return std::make_unique<ScopePrefixer>(name, *this);
}

} // namespace Stats
} // namespace Envoy
