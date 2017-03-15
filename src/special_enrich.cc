#include "special_enrich.h"

namespace cycvt {

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
special_enrich::special_enrich(cyclus::Context* ctx) : cyclus::Facility(ctx) {}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
std::string special_enrich::str() {
  return Facility::str();
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void special_enrich::Tick() {}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void special_enrich::Tock() {}

// WARNING! Do not change the following this function!!! This enables your
// archetype to be dynamically loaded and any alterations will cause your
// archetype to fail.
extern "C" cyclus::Agent* Constructspecial_enrich(cyclus::Context* ctx) {
  return new special_enrich(ctx);
}

}  // namespace cycvt
