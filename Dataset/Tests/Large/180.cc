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

NS_LOG_COMPONENT_DEFINE("AdhocAodvExampleTest");

// Test the creation of nodes
void TestNodeCreation() {
    NodeContainer nodes;
    nodes.Create(3);
    
    NS_TEST_ASSERT_MSG_EQ(nodes.GetN(), 3, "Incorrect number of nodes created");
}

// Test the Wi-Fi channel and physical layer setup
void TestWifiPhyAndChannelSetup() {
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());
    
    // Verify that the channel is set up correctly
    NS_TEST_ASSERT_MSG_NE(wifiChannel.GetChannel(), nullptr, "Wi-Fi channel was not created correctly");
    NS_TEST_ASSERT_MSG_NE(wifiPhy.GetChannel(), nullptr, "Wi-Fi PHY layer was not set correctly");
}

// Test the AODV routing protocol installation
void TestAodvRouting() {
    NodeContainer nodes;
    nodes.Create(3);

    AodvHelper aodv;
    InternetStackHelper internet;
    internet.SetRoutingHelper(aodv);
    internet.Install(nodes);
    
    // Verify that the AODV routing protocol is installed
    Ptr<Ipv4> ipv4 = nodes.Get(0)->GetObject<Ipv4>();
    NS_TEST_ASSERT_MSG_NE(ipv4->GetRoutingProtocol(), nullptr, "AODV routing protocol was not installed correctly");
}

// Test IP address assignment
void TestIpAddressAssignment() {
    NodeContainer nodes;
    nodes.Create(3);

    // Set up Wi-Fi and internet stack
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiHelper wifi;
    wifi.SetRemoteStationManager("ns3::AarfWifiManager");

    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    AodvHelper aodv;
    InternetStackHelper internet;
    internet.SetRoutingHelper(aodv);
    internet.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);
    
    // Verify IP address assignment
    NS_TEST_ASSERT_MSG_EQ(interfaces.GetAddress(0), Ipv4Address("10.1.1.1"), "Incorrect IP address for node 0");
    NS_TEST_ASSERT_MSG_EQ(interfaces.GetAddress(1), Ipv4Address("10.1.1.2"), "Incorrect IP address for node 1");
    NS_TEST_ASSERT_MSG_EQ(interfaces.GetAddress(2), Ipv4Address("10.1.1.3"), "Incorrect IP address for node 2");
}

// Test UDP application installation
void TestUdpApplications() {
    NodeContainer nodes;
    nodes.Create(3);
    
    uint16_t port = 4000;
    UdpServerHelper udpServer(port);
    ApplicationContainer serverApp = udpServer.Install(nodes.Get(1));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    UdpClientHelper udpClient(Ipv4Address("10.1.1.2"), port);
    udpClient.SetAttribute("MaxPackets", UintegerValue(100));
    udpClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    udpClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = udpClient.Install(nodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));
    
    // Verify that both server and client applications are installed correctly
    NS_TEST_ASSERT_MSG_EQ(serverApp.GetN(), 1, "UDP server application was not installed correctly");
    NS_TEST_ASSERT_MSG_EQ(clientApp.GetN(), 1, "UDP client application was not installed correctly");
}

// Test NetAnim visualization setup
void TestNetAnimVisualization() {
    NodeContainer nodes;
    nodes.Create(3);

    // Set up NetAnim for visualization
    AnimationInterface anim("aodv_netanim_test.xml");

    // Set constant positions for NetAnim visualization
    anim.SetConstantPosition(nodes.Get(0), 0.0, 0.0); // Node 0 (Client)
    anim.SetConstantPosition(nodes.Get(1), 5.0, 5.0); // Node 1 (Server)
    anim.SetConstantPosition(nodes.Get(2), 2.5, 2.5); // Node 2 (Relay)

    anim.EnablePacketMetadata(true);
    
    // Verify that the NetAnim XML file is generated and positions are set
    NS_TEST_ASSERT_MSG_EQ(anim.GetFileName(), "aodv_netanim_test.xml", "NetAnim file was not created correctly");
    NS_TEST_ASSERT_MSG_EQ(anim.GetNodePosition(nodes.Get(0)).x, 0.0, "Node 0 position was not set correctly in NetAnim");
    NS_TEST_ASSERT_MSG_EQ(anim.GetNodePosition(nodes.Get(1)).x, 5.0, "Node 1 position was not set correctly in NetAnim");
    NS_TEST_ASSERT_MSG_EQ(anim.GetNodePosition(nodes.Get(2)).x, 2.5, "Node 2 position was not set correctly in NetAnim");
}

// Main function to run all the unit tests
int main(int argc, char *argv[]) {
    // Run the unit tests
    TestNodeCreation();
    TestWifiPhyAndChannelSetup();
    TestAodvRouting();
    TestIpAddressAssignment();
    TestUdpApplications();
    TestNetAnimVisualization();

    return 0;
}
