#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/test.h" // Include the test header for unit tests

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiNetAnimTest");

class WifiNetAnimTest : public ns3::TestCase
{
public:
    WifiNetAnimTest() : TestCase("Wifi NetAnim Example Test") {}
    virtual ~WifiNetAnimTest() {}

    virtual void DoRun() {
        // Setup code from the original program
        NodeContainer wifiStaNodes;
        wifiStaNodes.Create(2);
        NodeContainer wifiApNode;
        wifiApNode.Create(1);

        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
        phy.SetChannel(channel.Create());

        WifiHelper wifi;
        wifi.SetStandard(WIFI_STANDARD_80211b);
        wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue("DsssRate1Mbps"), "ControlMode", StringValue("DsssRate1Mbps"));

        WifiMacHelper mac;
        Ssid ssid = Ssid("ns3-wifi");

        mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));
        NetDeviceContainer staDevices = wifi.Install(phy, mac, wifiStaNodes);

        mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
        NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiApNode);

        MobilityHelper mobility;
        mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                      "MinX", DoubleValue(0.0),
                                      "MinY", DoubleValue(0.0),
                                      "DeltaX", DoubleValue(5.0),
                                      "DeltaY", DoubleValue(10.0),
                                      "GridWidth", UintegerValue(3),
                                      "LayoutType", StringValue("RowFirst"));

        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        mobility.Install(wifiStaNodes);
        mobility.Install(wifiApNode);

        InternetStackHelper stack;
        stack.Install(wifiStaNodes);
        stack.Install(wifiApNode);

        Ipv4AddressHelper address;
        address.SetBase("10.1.3.0", "255.255.255.0");
        Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);
        Ipv4InterfaceContainer apInterface = address.Assign(apDevice);

        UdpEchoServerHelper echoServer(9);
        ApplicationContainer serverApp = echoServer.Install(wifiApNode.Get(0));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        UdpEchoClientHelper echoClient(apInterface.GetAddress(0), 9);
        echoClient.SetAttribute("MaxPackets", UintegerValue(1));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
        echoClient.SetAttribute("PacketSize", UintegerValue(1024));

        ApplicationContainer clientApp = echoClient.Install(wifiStaNodes.Get(0));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));

        AnimationInterface anim("wifi_netanim.xml");
        anim.SetConstantPosition(wifiStaNodes.Get(0), 0, 0);
        anim.SetConstantPosition(wifiStaNodes.Get(1), 5, 0);
        anim.SetConstantPosition(wifiApNode.Get(0), 2.5, 5);
        anim.EnablePacketMetadata(true);

        // Run the simulation
        Simulator::Stop(Seconds(10.0));
        Simulator::Run();
        Simulator::Destroy();
    }
};

// Test Wi-Fi setup
void TestWifiSetup() {
    WifiNetAnimTest test;
    test.Run();
    NS_TEST_ASSERT_MSG_EQ(test.GetTestStatus(), TestCase::TEST_SUCCESS, "Wi-Fi setup failed!");
}

// Test IP Address Assignment
void TestIpAddressAssignment() {
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(2);
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue("DsssRate1Mbps"), "ControlMode", StringValue("DsssRate1Mbps"));

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns3-wifi");

    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer staDevices = wifi.Install(phy, mac, wifiStaNodes);

    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiApNode);

    Ipv4AddressHelper address;
    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);
    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);

    // Check if the IP address has been assigned properly
    NS_TEST_ASSERT_MSG_EQ(staInterfaces.GetAddress(0), Ipv4Address("10.1.3.1"), "Incorrect IP assigned to STA 1");
    NS_TEST_ASSERT_MSG_EQ(apInterface.GetAddress(0), Ipv4Address("10.1.3.2"), "Incorrect IP assigned to AP");
}

// Test UDP Echo Server and Client
void TestUdpEchoServerAndClient() {
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(2);
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApp = echoServer.Install(wifiApNode.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient(Ipv4Address("10.1.3.2"), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(1));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = echoClient.Install(wifiStaNodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Test the application installation
    NS_TEST_ASSERT_MSG_EQ(serverApp.GetN(), 1, "UDP Echo Server not installed correctly!");
    NS_TEST_ASSERT_MSG_EQ(clientApp.GetN(), 1, "UDP Echo Client not installed correctly!");
}

// Test mobility model
void TestMobilityModel() {
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(2);
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(10.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiStaNodes);
    mobility.Install(wifiApNode);

    // Test the positions of the nodes
    Ptr<ConstantPositionMobilityModel> sta1Position = wifiStaNodes.Get(0)->GetObject<ConstantPositionMobilityModel>();
    Ptr<ConstantPositionMobilityModel> sta2Position = wifiStaNodes.Get(1)->GetObject<ConstantPositionMobilityModel>();

    NS_TEST_ASSERT_MSG_EQ(sta1Position->GetPosition().x, 0.0, "STA1 position not set correctly!");
    NS_TEST_ASSERT_MSG_EQ(sta2Position->GetPosition().x, 5.0, "STA2 position not set correctly!");
}

// Main function to run all tests
int main(int argc, char *argv[]) {
    TestWifiSetup();
    TestIpAddressAssignment();
    TestUdpEchoServerAndClient();
    TestMobilityModel();

    return 0;
}
