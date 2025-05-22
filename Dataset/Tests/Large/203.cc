#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("VanetExampleTest");

// Test 1: Node creation and device installation
void TestNodeCreationAndDeviceInstallation()
{
    // Create nodes (5 vehicles)
    NodeContainer vehicles;
    vehicles.Create(5);

    NS_TEST_ASSERT_MSG_EQ(vehicles.GetN(), 5, "Failed to create five vehicles.");

    // Set up 802.11p Wi-Fi (vehicular communication)
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    Wifi80211pHelper wifi80211p = Wifi80211pHelper::Default();
    NqosWaveMacHelper mac = NqosWaveMacHelper::Default();
    NetDeviceContainer devices = wifi80211p.Install(phy, mac, vehicles);

    NS_TEST_ASSERT_MSG_EQ(devices.GetN(), 5, "Failed to install Wi-Fi devices on vehicles.");
}

// Test 2: IP address assignment
void TestIpAddressAssignment()
{
    NodeContainer vehicles;
    vehicles.Create(5);

    // Set up Wi-Fi devices
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());
    
    Wifi80211pHelper wifi80211p = Wifi80211pHelper::Default();
    NqosWaveMacHelper mac = NqosWaveMacHelper::Default();
    NetDeviceContainer devices = wifi80211p.Install(phy, mac, vehicles);

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(vehicles);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer vehicleInterfaces = ipv4.Assign(devices);

    NS_TEST_ASSERT_MSG_NE(vehicleInterfaces.GetAddress(0), Ipv4Address::GetAny(), "Failed to assign IP address to vehicle.");
}

// Test 3: UDP Echo application setup
void TestUdpEchoApplication()
{
    NodeContainer vehicles;
    vehicles.Create(5);

    // Set up Wi-Fi devices and IP addresses
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    Wifi80211pHelper wifi80211p = Wifi80211pHelper::Default();
    NqosWaveMacHelper mac = NqosWaveMacHelper::Default();
    NetDeviceContainer devices = wifi80211p.Install(phy, mac, vehicles);

    InternetStackHelper internet;
    internet.Install(vehicles);
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer vehicleInterfaces = ipv4.Assign(devices);

    // Set up UDP echo server on the first vehicle
    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApp = echoServer.Install(vehicles.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(20.0));

    NS_TEST_ASSERT_MSG_EQ(serverApp.Get(0)->GetStartTime().GetSeconds(), 1.0, "UDP echo server start time is incorrect.");

    // Set up UDP echo clients on other vehicles
    UdpEchoClientHelper echoClient(vehicleInterfaces.GetAddress(0), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(10));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(2.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps;
    for (uint32_t i = 1; i < vehicles.GetN(); ++i)
    {
        clientApps.Add(echoClient.Install(vehicles.Get(i)));
    }
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(20.0));

    for (uint32_t i = 0; i < vehicles.GetN() - 1; ++i)
    {
        NS_TEST_ASSERT_MSG_EQ(clientApps.Get(i)->GetStartTime().GetSeconds(), 2.0, "UDP echo client start time is incorrect.");
    }
}

// Test 4: Mobility model setup
void TestMobilityModel()
{
    NodeContainer vehicles;
    vehicles.Create(5);

    // Set up mobility for vehicles
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0));  // First vehicle position
    positionAlloc->Add(Vector(20.0, 0.0, 0.0)); // Second vehicle position
    positionAlloc->Add(Vector(40.0, 0.0, 0.0)); // Third vehicle position
    positionAlloc->Add(Vector(60.0, 0.0, 0.0)); // Fourth vehicle position
    positionAlloc->Add(Vector(80.0, 0.0, 0.0)); // Fifth vehicle position
    mobility.SetPositionAllocator(positionAlloc);
    mobility.Install(vehicles);

    // Set velocities for vehicles
    for (uint32_t i = 0; i < vehicles.GetN(); ++i)
    {
        Ptr<ConstantVelocityMobilityModel> mob = vehicles.Get(i)->GetObject<ConstantVelocityMobilityModel>();
        mob->SetVelocity(Vector(10.0 + i, 0.0, 0.0));
    }

    // Verify positions and velocities
    Vector pos = vehicles.Get(0)->GetObject<MobilityModel>()->GetPosition();
    NS_TEST_ASSERT_MSG_EQ(pos.x, 0.0, "First vehicle position incorrect.");

    for (uint32_t i = 1; i < vehicles.GetN(); ++i)
    {
        pos = vehicles.Get(i)->GetObject<MobilityModel>()->GetPosition();
        NS_TEST_ASSERT_MSG_EQ(pos.x, i * 20.0, "Vehicle position incorrect.");
    }
}

// Test 5: FlowMonitor statistics validation
void TestFlowMonitor()
{
    NodeContainer vehicles;
    vehicles.Create(5);

    // Set up Wi-Fi devices and IP addresses
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    Wifi80211pHelper wifi80211p = Wifi80211pHelper::Default();
    NqosWaveMacHelper mac = NqosWaveMacHelper::Default();
    NetDeviceContainer devices = wifi80211p.Install(phy, mac, vehicles);

    InternetStackHelper internet;
    internet.Install(vehicles);
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer vehicleInterfaces = ipv4.Assign(devices);

    // Set up applications
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApp = echoServer.Install(vehicles.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(20.0));

    UdpEchoClientHelper echoClient(vehicleInterfaces.GetAddress(0), 9);
    ApplicationContainer clientApps;
    for (uint32_t i = 1; i < vehicles.GetN(); ++i)
    {
        clientApps.Add(echoClient.Install(vehicles.Get(i)));
    }
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(20.0));

    // Install FlowMonitor
    FlowMonitorHelper flowMonitorHelper;
    Ptr<FlowMonitor> flowMonitor = flowMonitorHelper.InstallAll();

    // Run the simulation
    Simulator::Stop(Seconds(20.0));
    Simulator::Run();

    // Validate FlowMonitor statistics
    flowMonitor->CheckForLostPackets();
    FlowMonitor::FlowStatsContainer stats = flowMonitor->GetFlowStats();
    NS_TEST_ASSERT_MSG_NE(stats.size(), 0, "FlowMonitor did not capture any traffic.");

    for (auto it = stats.begin(); it != stats.end(); ++it)
    {
        NS_TEST_ASSERT_MSG_GT(it->second.txPackets, 0, "No packets transmitted.");
        NS_TEST_ASSERT_MSG_GT(it->second.rxPackets, 0, "No packets received.");
        NS_TEST_ASSERT_MSG_LT(it->second.delaySum.GetSeconds(), 1.0, "End-to-End delay too high.");
    }

    Simulator::Destroy();
}

// Main function to run all tests
int main(int argc, char *argv[])
{
    TestNodeCreationAndDeviceInstallation();
    TestIpAddressAssignment();
    TestUdpEchoApplication();
    TestMobilityModel();
    TestFlowMonitor();

    NS_LOG_UNCOND("All tests passed successfully.");
    return 0;
}

