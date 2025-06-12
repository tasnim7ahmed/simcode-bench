#include "ns3/test.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/nr-module.h"

using namespace ns3;

class NrSimulationTestSuite : public TestCase
{
public:
    NrSimulationTestSuite() : TestCase("Test 5G NR Network Simulation") {}
    virtual ~NrSimulationTestSuite
