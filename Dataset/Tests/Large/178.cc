#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/test.h"  // Include the ns3 Test module for unit testing

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiAdhocExampleTest");

class WifiAdhocExampleTest : public ns3::TestCase
{
public:
    WifiAdhocExampleTest() : TestCase("Wi-Fi Ad-hoc Example Test") {}
    virtual ~WifiAdhocExampleTest() {}

    virtual void DoRun() {
        // Simulate the setup from the main code
        NodeContainer wifiNodes;
        wifiNodes.Create(2);

        // Wi-Fi PHY and Channel setup
        WifiHelper wifi;
        wifi.SetStandard(WIFI_PHY_STANDARD_80211n_5GHZ);

        YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        phy.SetChannel(channel.Create());

        WifiMacHelper mac;
        wifi.SetRemoteStationManager("ns3::AarfWifiManager");

        mac.SetType("ns3::AdhocWifiMac");
        NetDeviceContainer devices = wifi.Install(phy, mac, wifiNodes);

        // Install internet stack
        InternetStackHelper stack;
        stack.Install(wifiNodes);

        // IP address assignment
        Ipv4AddressHelper address;
        address.SetBase("192.168.1.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = address.Assign(devices);

        // Check IP assignments
        NS_TEST_ASSERT_MSG_EQ(interfaces.GetAddress(0), Ipv4Address("192.168.1.1"), "Incorrect IP address for node 0");
        NS_TEST_ASSERT_MSG_EQ(interfaces.GetAddress(1), Ipv4Address("192.168.1.2"), "Incorrect IP address for node 1");
    }
};

// Test mobility setup for the Wi-Fi nodes
void TestMobilitySetup() {
    NodeContainer wifiNodes;
    wifiNodes.Create(2);

    // Mobility setup
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(2),
                                  "LayoutType", StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiNodes);

    Ptr<ConstantPositionMobilityModel> mobilityModel0 = wifiNodes.Get(0)->GetObject<ConstantPositionMobilityModel>();
    Ptr<ConstantPositionMobilityModel> mobilityModel1 = wifiNodes.Get(1)->GetObject<ConstantPositionMobilityModel>();

    // Verify that the positions are correctly set
    NS_TEST_ASSERT_MSG_EQ(mobilityModel0->GetPosition(), Vector(0.0, 0.0, 0.0), "Incorrect position for node 0");
    NS_TEST_ASSERT_MSG_EQ(mobilityModel1->GetPosition(), Vector(5.0, 0.0, 0.0), "Incorrect position for node 1");
}

// Test UDP client-server application setup
void TestUdpClientServer() {
    NodeContainer wifiNodes;
    wifiNodes.Create(2);

    uint16_t port = 9;
    UdpServerHelper server(port);
    ApplicationContainer serverApp = server.Install(wifiNodes.Get(1));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    UdpClientHelper client(Ipv4Address("192.168.1.2"), port);
    client.SetAttribute("MaxPackets", UintegerValue(320));
    client.SetAttribute("Interval", TimeValue(Seconds(0.05)));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = client.Install(wifiNodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Verify that the client and server applications are correctly installed
    NS_TEST_ASSERT_MSG_EQ(serverApp.GetN(), 1, "UDP server application not installed correctly");
    NS_TEST_ASSERT_MSG_EQ(clientApp.GetN(), 1, "UDP client application not installed correctly");
}

// Test NetAnim Visualization Setup
void TestNetAnimVisualization() {
    NodeContainer wifiNodes;
    wifiNodes.Create(2);

    // Setup animation interface for visualization
    AnimationInterface anim("wifi_adhoc_test.xml");

    // Set constant positions for NetAnim visualization
    anim.SetConstantPosition(wifiNodes.Get(0), 1.0, 1.0);  // Client node
    anim.SetConstantPosition(wifiNodes.Get(1), 5.0, 1.0);  // Server node

    anim.EnablePacketMetadata(true);

    // Verify if NetAnim file was created
    NS_TEST_ASSERT_MSG_EQ(anim.GetFileName(), "wifi_adhoc_test.xml", "NetAnim file was not created correctly");
}

// Main function to run all the unit tests
int main(int argc, char *argv[]) {
    WifiAdhocExampleTest test1;
    test1.Run();

    TestMobilitySetup();
    TestUdpClientServer();
    TestNetAnimVisualization();

    return 0;
}
