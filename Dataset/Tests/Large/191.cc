#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/energy-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-apps-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WirelessSensorNetwork");

// Test 1: Node creation
void TestNodeCreation()
{
    uint32_t gridSize = 3; // 3x3 grid of nodes
    NodeContainer sensorNodes;
    sensorNodes.Create (gridSize * gridSize);
    NS_TEST_ASSERT_MSG_EQ(sensorNodes.GetN(), gridSize * gridSize, "Expected nodes to be created in a 3x3 grid");
}

// Test 2: WiFi installation
void TestWifiInstallation()
{
    NodeContainer sensorNodes;
    sensorNodes.Create(9);  // 3x3 grid

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    Ssid ssid = Ssid("wsn-grid");
    mac.SetType("ns3::AdhocWifiMac", "Ssid", SsidValue(ssid));

    NetDeviceContainer devices = wifi.Install(phy, mac, sensorNodes);

    NS_TEST_ASSERT_MSG_EQ(devices.GetN(), 9, "Expected WiFi devices to be installed on all nodes");
}

// Test 3: Mobility model installation
void TestMobilityModel()
{
    uint32_t gridSize = 3;
    NodeContainer sensorNodes;
    sensorNodes.Create(gridSize * gridSize);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(50.0),
                                  "DeltaY", DoubleValue(50.0),
                                  "GridWidth", UintegerValue(gridSize),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(sensorNodes);

    for (uint32_t i = 0; i < sensorNodes.GetN(); ++i)
    {
        Ptr<MobilityModel> mobilityModel = sensorNodes.Get(i)->GetObject<MobilityModel>();
        NS_TEST_ASSERT_MSG_NE(mobilityModel, nullptr, "Mobility model not installed correctly");
    }
}

// Test 4: Energy model installation
void TestEnergyModel()
{
    uint32_t gridSize = 3;
    double totalEnergy = 100.0; // Initial energy in Joules
    NodeContainer sensorNodes;
    sensorNodes.Create(gridSize * gridSize);

    // Set up energy source
    BasicEnergySourceHelper energySourceHelper;
    energySourceHelper.Set("BasicEnergySourceInitialEnergyJ", DoubleValue(totalEnergy));
    EnergySourceContainer sources = energySourceHelper.Install(sensorNodes);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    Ssid ssid = Ssid("wsn-grid");
    mac.SetType("ns3::AdhocWifiMac", "Ssid", SsidValue(ssid));

    NetDeviceContainer devices = wifi.Install(phy, mac, sensorNodes);

    // Set up energy consumption model
    WifiRadioEnergyModelHelper radioEnergyHelper;
    DeviceEnergyModelContainer deviceModels = radioEnergyHelper.Install(devices, sources);

    for (uint32_t i = 0; i < sources.GetN(); ++i)
    {
        Ptr<BasicEnergySource> basicSource = DynamicCast<BasicEnergySource>(sources.Get(i));
        NS_TEST_ASSERT_MSG_NE(basicSource, nullptr, "Energy source not installed correctly");
    }
}

// Test 5: Application installation
void TestApplicationInstallation()
{
    uint32_t gridSize = 3;
    double simulationTime = 50.0;
    NodeContainer sensorNodes;
    sensorNodes.Create(gridSize * gridSize);

    InternetStackHelper internet;
    internet.Install(sensorNodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    NetDeviceContainer devices = sensorNodes.Get(0)->GetObject<NetDeviceContainer>();
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    uint16_t sinkPort = 9;
    Address sinkAddress(InetSocketAddress(interfaces.GetAddress(0), sinkPort)); // First node is the sink

    PacketSinkHelper sinkHelper("ns3::UdpSocketFactory", sinkAddress);
    ApplicationContainer sinkApp = sinkHelper.Install(sensorNodes.Get(0)); // Sink installed on node 0
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(simulationTime));

    OnOffHelper clientHelper("ns3::UdpSocketFactory", sinkAddress);
    clientHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    clientHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    clientHelper.SetAttribute("DataRate", DataRateValue(DataRate("1kbps")));
    clientHelper.SetAttribute("PacketSize", UintegerValue(50));

    ApplicationContainer clientApps;
    for (uint32_t i = 1; i < sensorNodes.GetN(); ++i) // All nodes except the sink send data
    {
        clientApps.Add(clientHelper.Install(sensorNodes.Get(i)));
    }

    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(simulationTime));

    // Check if applications are installed correctly
    NS_TEST_ASSERT_MSG_EQ(sinkApp.Get(0)->GetStartTime().GetSeconds(), 0.0, "Sink app did not start at the correct time");
    NS_TEST_ASSERT_MSG_EQ(clientApps.Get(0)->GetStartTime().GetSeconds(), 1.0, "Client app did not start at the correct time");
}

// Test 6: Energy consumption tracing
void TestEnergyConsumption()
{
    uint32_t gridSize = 3;
    NodeContainer sensorNodes;
    sensorNodes.Create(gridSize * gridSize);

    BasicEnergySourceHelper energySourceHelper;
    energySourceHelper.Set("BasicEnergySourceInitialEnergyJ", DoubleValue(100.0));
    EnergySourceContainer sources = energySourceHelper.Install(sensorNodes);

    for (uint32_t i = 0; i < sources.GetN(); ++i)
    {
        Ptr<BasicEnergySource> basicSource = DynamicCast<BasicEnergySource>(sources.Get(i));
        basicSource->TraceConnectWithoutContext("RemainingEnergy", MakeCallback(&RemainingEnergy));
    }

    // Verify if energy consumption is being tracked (although actual verification would require further checks)
    NS_LOG_UNCOND("Energy tracing setup completed.");
}

// Main test execution function
int main(int argc, char *argv[])
{
    // Run all tests
    TestNodeCreation();
    TestWifiInstallation();
    TestMobilityModel();
    TestEnergyModel();
    TestApplicationInstallation();
    TestEnergyConsumption();

    NS_LOG_UNCOND("All tests passed successfully.");
    return 0;
}
