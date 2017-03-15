#include <gtest/gtest.h>

#include "special_enrich.h"

#include "agent_tests.h"
#include "context.h"
#include "facility_tests.h"
#inlcude "pyhooks.h"

using cycvt::special_enrich;

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
class special_enrichTest : public ::testing::Test {
 protected:
  cyclus::TestContext tc;
  special_enrich* facility;

  virtual void SetUp() {
    cyclus::PyStart();
    facility = new special_enrich(tc.get());
  }

  virtual void TearDown() {
    delete facility;
    cyclus::PyStop();
  }
};

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(special_enrichTest, InitialState) {
  // Test things about the initial state of the facility here
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(special_enrichTest, Print) {
  EXPECT_NO_THROW(std::string s = facility->str());
  // Test special_enrich specific aspects of the print method here
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(special_enrichTest, Tick) {
  ASSERT_NO_THROW(facility->Tick());
  // Test special_enrich specific behaviors of the Tick function here
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(special_enrichTest, Tock) {
  EXPECT_NO_THROW(facility->Tock());
  // Test special_enrich specific behaviors of the Tock function here
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// Do Not Touch! Below section required for connection with Cyclus
cyclus::Agent* special_enrichConstructor(cyclus::Context* ctx) {
  return new special_enrich(ctx);
}
// Required to get functionality in cyclus agent unit tests library
#ifndef CYCLUS_AGENT_TESTS_CONNECTED
int ConnectAgentTests();
static int cyclus_agent_tests_connected = ConnectAgentTests();
#define CYCLUS_AGENT_TESTS_CONNECTED cyclus_agent_tests_connected
#endif  // CYCLUS_AGENT_TESTS_CONNECTED
INSTANTIATE_TEST_CASE_P(special_enrich, FacilityTests,
                        ::testing::Values(&special_enrichConstructor));
INSTANTIATE_TEST_CASE_P(special_enrich, AgentTests,
                        ::testing::Values(&special_enrichConstructor));
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
