#include "ns3/test.h"
#include "ns3/log.h"
#include "ns3/node-container.h"
#include "ns3/wifi-helper.h"
#include "ns3/mobility-helper.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-address-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("Ns3UnitTests");

class Ns3SimulationTest : public TestCase {
public:
    Ns3SimulationTest() : TestCase("Ns3 Simulation Unit Test") {}
    virtual void DoRun();
};

void Ns3SimulationTest::DoRun() {
    // Test Node Creation
    NodeContainer nodes;
    nodes.Create(2);
    NS_TEST_ASSERT_MSG_EQ(nodes.GetN(), 2, "Node creation failed");

    // Test Wi-Fi Configuration
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n);
    NS_TEST_ASSERT_MSG_EQ(wifi.GetStandard(), WIFI_STANDARD_80211n, "Wi-Fi standard not set correctly");
    
    // Test Mobility Model
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(10.0),
                                  "GridWidth", UintegerValue(2),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);
    NS_TEST_ASSERT_MSG_EQ(nodes.Get(0)->GetObject<MobilityModel>() != nullptr, true, "Mobility model not installed");
    
    // Test Internet Stack Installation
    InternetStackHelper stack;
    stack.Install(nodes);
    NS_TEST_ASSERT_MSG_EQ(nodes.Get(0)->GetObject<Ipv4>() != nullptr, true, "Internet stack not installed");

    // Test IP Address Assignment
    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    NS_TEST_ASSERT_MSG_EQ(address.Assign(Ipv4InterfaceContainer()).Get(0) != Ipv4Address("0.0.0.0"), true, "IP Address assignment failed");
}

static Ns3SimulationTest ns3SimulationTestInstance;
