#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wave-module.h"
#include "ns3/mobility-module.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/test.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("V2VCommunicationExample");

// Test 1: Node Creation
void TestNodeCreation()
{
    NodeContainer vehicles;
    vehicles.Create(2);

    // Verify that 2 nodes were created
    NS_TEST_ASSERT_MSG_EQ(vehicles.GetN(), 2, "Expected 2 vehicle nodes");
}

// Test 2: Wi-Fi Device Installation
void TestWifiDeviceInstallation()
{
    NodeContainer vehicles;
    vehicles.Create(2);

    // Set up Wi-Fi for vehicular communication using Wave (802.11p)
    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    QosWaveMacHelper wifiMac = QosWaveMacHelper::Default();
    WaveHelper waveHelper = WaveHelper::Default();
    NetDeviceContainer devices = waveHelper.Install(wifiPhy, wifiMac, vehicles);

    // Verify that Wi-Fi devices are installed on both vehicles
    NS_TEST_ASSERT_MSG_EQ(devices.GetN(), 2, "Failed to install Wi-Fi devices on the vehicles");
}

// Test 3: Mobility Model Setup
void TestMobilitySetup()
{
    NodeContainer vehicles;
    vehicles.Create(2);

    // Set up mobility model for the vehicles
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");

    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0));  // Position for vehicle 1
    positionAlloc->Add(Vector(50.0, 0.0, 0.0));  // Position for vehicle 2
    mobility.SetPositionAllocator(positionAlloc);
    mobility.Install(vehicles);

    // Verify positions of the vehicles
    Ptr<MobilityModel> mobilityModel0 = vehicles.Get(0)->GetObject<MobilityModel>();
    Ptr<MobilityModel> mobilityModel1 = vehicles.Get(1)->GetObject<MobilityModel>();
    NS_TEST_ASSERT_MSG_EQ(mobilityModel0->GetPosition(), Vector(0.0, 0.0, 0.0), "Vehicle 1 position incorrect");
    NS_TEST_ASSERT_MSG_EQ(mobilityModel1->GetPosition(), Vector(50.0, 0.0, 0.0), "Vehicle 2 position incorrect");
}

// Test 4: IP Address Assignment
void TestIpAddressAssignment()
{
    NodeContainer vehicles;
    vehicles.Create(2);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    QosWaveMacHelper wifiMac = QosWaveMacHelper::Default();
    WaveHelper waveHelper = WaveHelper::Default();
    NetDeviceContainer devices = waveHelper.Install(wifiPhy, wifiMac, vehicles);

    // Install the internet stack and assign IP addresses
    InternetStackHelper internet;
    internet.Install(vehicles);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // Verify that IP addresses have been assigned
    NS_TEST_ASSERT_MSG_NE(interfaces.GetAddress(0), Ipv4Address("0.0.0.0"), "Failed to assign IP to vehicle 1");
    NS_TEST_ASSERT_MSG_NE(interfaces.GetAddress(1), Ipv4Address("0.0.0.0"), "Failed to assign IP to vehicle 2");
}

// Test 5: UDP Server Installation
void TestUdpServerInstallation()
{
    NodeContainer vehicles;
    vehicles.Create(2);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    QosWaveMacHelper wifiMac = QosWaveMacHelper::Default();
    WaveHelper waveHelper = WaveHelper::Default();
    NetDeviceContainer devices = waveHelper.Install(wifiPhy, wifiMac, vehicles);

    // Install the internet stack and assign IP addresses
    InternetStackHelper internet;
    internet.Install(vehicles);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // Set up UDP server on vehicle 2 (Node 1)
    uint16_t port = 4000;
    UdpServerHelper udpServer(port);
    ApplicationContainer serverApp = udpServer.Install(vehicles.Get(1));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Verify that the UDP server was installed successfully
    NS_TEST_ASSERT_MSG_EQ(serverApp.GetN(), 1, "Failed to install UDP server on vehicle 2");
}

// Test 6: UDP Client Installation
void TestUdpClientInstallation()
{
    NodeContainer vehicles;
    vehicles.Create(2);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    QosWaveMacHelper wifiMac = QosWaveMacHelper::Default();
    WaveHelper waveHelper = WaveHelper::Default();
    NetDeviceContainer devices = waveHelper.Install(wifiPhy, wifiMac, vehicles);

    // Install the internet stack and assign IP addresses
    InternetStackHelper internet;
    internet.Install(vehicles);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // Set up UDP client on vehicle 1 (Node 0)
    uint16_t port = 4000;
    UdpClientHelper udpClient(interfaces.GetAddress(1), port);
    udpClient.SetAttribute("MaxPackets", UintegerValue(100));
    udpClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    udpClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = udpClient.Install(vehicles.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Verify that the UDP client was installed successfully
    NS_TEST_ASSERT_MSG_EQ(clientApp.GetN(), 1, "Failed to install UDP client on vehicle 1");
}

// Test 7: Flow Monitor and Statistics
void TestFlowMonitorStatistics()
{
    NodeContainer vehicles;
    vehicles.Create(2);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    QosWaveMacHelper wifiMac = QosWaveMacHelper::Default();
    WaveHelper waveHelper = WaveHelper::Default();
    NetDeviceContainer devices = waveHelper.Install(wifiPhy, wifiMac, vehicles);

    // Install the internet stack and assign IP addresses
    InternetStackHelper internet;
    internet.Install(vehicles);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // Set up UDP server on vehicle 2 (Node 1)
    uint16_t port = 4000;
    UdpServerHelper udpServer(port);
    ApplicationContainer serverApp = udpServer.Install(vehicles.Get(1));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Set up UDP client on vehicle 1 (Node 0)
    UdpClientHelper udpClient(interfaces.GetAddress(1), port);
    udpClient.SetAttribute("MaxPackets", UintegerValue(100));
    udpClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    udpClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = udpClient.Install(vehicles.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Set up Flow Monitor to track statistics
    FlowMonitorHelper flowHelper;
    Ptr<FlowMonitor> monitor = flowHelper.InstallAll();

    // Run the simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    // Verify flow statistics
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowHelper.GetClassifier()));
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    for (auto i = stats.begin(); i != stats.end(); ++i)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        NS_TEST_ASSERT_MSG_GT(i->second.rxPackets, 0, "No packets received on flow");
    }

    // Clean up
    Simulator::Destroy();
}

// Main function to run all tests
int main(int argc, char *argv[])
{
    // Run all tests
    TestNodeCreation();
    TestWifiDeviceInstallation();
    TestMobilitySetup();
    TestIpAddressAssignment();
    TestUdpServerInstallation();
    TestUdpClientInstallation();
    TestFlowMonitorStatistics();

    NS_LOG_UNCOND("All tests passed successfully.");
    return 0;
}
