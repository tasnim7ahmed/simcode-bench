#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/aodv-helper.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/test.h"  // Include the ns3 Test module for unit testing

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("VANETExampleTest");

// Test the creation of vehicles (nodes)
void TestNodeCreation() {
    NodeContainer vehicles;
    vehicles.Create(2);

    NS_TEST_ASSERT_MSG_EQ(vehicles.GetN(), 2, "Incorrect number of vehicles created");
}

// Test the WAVE (802.11p) PHY and MAC layer setup
void TestWavePhyAndMacSetup() {
    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    NqosWaveMacHelper wifiMac = NqosWaveMacHelper::Default();
    Wifi80211pHelper waveHelper = Wifi80211pHelper::Default();
    NodeContainer vehicles;
    vehicles.Create(2);
    NetDeviceContainer devices = waveHelper.Install(wifiPhy, wifiMac, vehicles);

    // Verify that the Wi-Fi PHY and MAC layers are set up correctly
    NS_TEST_ASSERT_MSG_NE(devices.Get(0), nullptr, "Device 0 was not created correctly");
    NS_TEST_ASSERT_MSG_NE(devices.Get(1), nullptr, "Device 1 was not created correctly");
}

// Test the mobility setup (constant velocity model)
void TestMobilitySetup() {
    NodeContainer vehicles;
    vehicles.Create(2);

    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.Install(vehicles);

    Ptr<ConstantVelocityMobilityModel> mobility1 = vehicles.Get(0)->GetObject<ConstantVelocityMobilityModel>();
    Ptr<ConstantVelocityMobilityModel> mobility2 = vehicles.Get(1)->GetObject<ConstantVelocityMobilityModel>();

    // Verify that the mobility models were installed correctly
    NS_TEST_ASSERT_MSG_NE(mobility1, nullptr, "Mobility model for vehicle 1 was not installed correctly");
    NS_TEST_ASSERT_MSG_NE(mobility2, nullptr, "Mobility model for vehicle 2 was not installed correctly");
}

// Test IP address assignment to vehicles
void TestIpAddressAssignment() {
    NodeContainer vehicles;
    vehicles.Create(2);

    // Set up WAVE and Internet stack
    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    NqosWaveMacHelper wifiMac = NqosWaveMacHelper::Default();
    Wifi80211pHelper waveHelper = Wifi80211pHelper::Default();
    NetDeviceContainer devices = waveHelper.Install(wifiPhy, wifiMac, vehicles);

    InternetStackHelper internet;
    internet.Install(vehicles);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Verify IP address assignment
    NS_TEST_ASSERT_MSG_EQ(interfaces.GetAddress(0), Ipv4Address("10.1.1.1"), "Incorrect IP address for vehicle 1");
    NS_TEST_ASSERT_MSG_EQ(interfaces.GetAddress(1), Ipv4Address("10.1.1.2"), "Incorrect IP address for vehicle 2");
}

// Test UDP application installation on vehicles
void TestUdpApplications() {
    NodeContainer vehicles;
    vehicles.Create(2);

    uint16_t port = 4000;
    UdpServerHelper udpServer(port);
    ApplicationContainer serverApp = udpServer.Install(vehicles.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    UdpClientHelper udpClient(Ipv4Address("10.1.1.1"), port);
    udpClient.SetAttribute("MaxPackets", UintegerValue(10));
    udpClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    udpClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = udpClient.Install(vehicles.Get(1));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Verify that the UDP server and client applications are installed correctly
    NS_TEST_ASSERT_MSG_EQ(serverApp.GetN(), 1, "UDP server application was not installed correctly");
    NS_TEST_ASSERT_MSG_EQ(clientApp.GetN(), 1, "UDP client application was not installed correctly");
}

// Test NetAnim visualization setup
void TestNetAnimVisualization() {
    NodeContainer vehicles;
    vehicles.Create(2);

    // Set up NetAnim for visualization
    AnimationInterface anim("vanet_netanim_test.xml");

    // Set constant positions for NetAnim visualization
    anim.SetConstantPosition(vehicles.Get(0), 0.0, 0.0);  // Vehicle 1
    anim.SetConstantPosition(vehicles.Get(1), 100.0, 0.0); // Vehicle 2

    anim.EnablePacketMetadata(true);

    // Verify that the NetAnim XML file is generated and positions are set
    NS_TEST_ASSERT_MSG_EQ(anim.GetFileName(), "vanet_netanim_test.xml", "NetAnim file was not created correctly");
    NS_TEST_ASSERT_MSG_EQ(anim.GetNodePosition(vehicles.Get(0)).x, 0.0, "Vehicle 1 position was not set correctly in NetAnim");
    NS_TEST_ASSERT_MSG_EQ(anim.GetNodePosition(vehicles.Get(1)).x, 100.0, "Vehicle 2 position was not set correctly in NetAnim");
}

// Main function to run all the unit tests
int main(int argc, char *argv[]) {
    // Run the unit tests
    TestNodeCreation();
    TestWavePhyAndMacSetup();
    TestMobilitySetup();
    TestIpAddressAssignment();
    TestUdpApplications();
    TestNetAnimVisualization();

    return 0;
}
