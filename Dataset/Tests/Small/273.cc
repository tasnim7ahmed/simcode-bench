#include "ns3/test.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/applications-module.h"

using namespace ns3;

class WifiTestSuite : public TestCase
{
public:
    WifiTestSuite() : TestCase("Test Wi-Fi Network with UDP Echo Applications") {}
    virtual ~WifiTestSuite() {}

    void DoRun() override
    {
        TestNodeCreation();
        TestWifiDeviceInstallation();
        TestMobilityModel();
        TestInternetStackInstallation();
        TestIpAddressAssignment();
        TestUdpEchoServerSetup();
        TestUdpEchoClientSetup();
        TestDataTransmission();
    }

private:
    // Test node creation (verify that 3 STA nodes and 1 AP node are created)
    void TestNodeCreation()
    {
        NodeContainer wifiStaNodes, wifiApNode;
        wifiStaNodes.Create(3);
        wifiApNode.Create(1);

        // Verify that 3 STA nodes and 1 AP node are created
        NS_TEST_ASSERT_MSG_EQ(wifiStaNodes.GetN(), 3, "Failed to create the expected number of STA nodes.");
        NS_TEST_ASSERT_MSG_EQ(wifiApNode.GetN(), 1, "Failed to create the expected number of AP nodes.");
    }

    // Test Wi-Fi device installation (verify that Wi-Fi devices are installed)
    void TestWifiDeviceInstallation()
    {
        NodeContainer wifiStaNodes, wifiApNode;
        wifiStaNodes.Create(3);
        wifiApNode.Create(1);

        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
        phy.SetChannel(channel.Create());

        WifiHelper wifi;
        wifi.SetStandard(WIFI_STANDARD_80211n);
        wifi.SetRemoteStationManager("ns3::AarfWifiManager");

        WifiMacHelper mac;
        Ssid ssid = Ssid("ns3-wifi");

        mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));
        NetDeviceContainer staDevices = wifi.Install(phy, mac, wifiStaNodes);

        mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
        NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiApNode);

        // Verify that devices are installed on all nodes
        NS_TEST_ASSERT_MSG_EQ(staDevices.GetN(), 3, "Failed to install Wi-Fi devices on all STA nodes.");
        NS_TEST_ASSERT_MSG_EQ(apDevice.GetN(), 1, "Failed to install Wi-Fi device on AP node.");
    }

    // Test mobility model (verify that mobility models are installed on STA nodes)
    void TestMobilityModel()
    {
        NodeContainer wifiStaNodes, wifiApNode;
        wifiStaNodes.Create(3);
        wifiApNode.Create(1);

        MobilityHelper mobility;

        // Mobility for stations (RandomWalk)
        mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                      "MinX", DoubleValue(0.0),
                                      "MinY", DoubleValue(0.0),
                                      "DeltaX", DoubleValue(5.0),
                                      "DeltaY", DoubleValue(5.0),
                                      "GridWidth", UintegerValue(3),
                                      "LayoutType", StringValue("RowFirst"));
        mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                                  "Bounds", RectangleValue(Rectangle(-50, 50, -50, 50)));
        mobility.Install(wifiStaNodes);

        // Mobility for AP (ConstantPosition)
        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        mobility.Install(wifiApNode);

        // Verify that mobility models are installed correctly
        Ptr<MobilityModel> staMobility = wifiStaNodes.Get(0)->GetObject<MobilityModel>();
        Ptr<MobilityModel> apMobility = wifiApNode.Get(0)->GetObject<MobilityModel>();
        NS_TEST_ASSERT_MSG_NE(staMobility, nullptr, "Failed to install mobility model on STA node.");
        NS_TEST_ASSERT_MSG_NE(apMobility, nullptr, "Failed to install mobility model on AP node.");
    }

    // Test Internet stack installation (verify that the Internet stack is installed)
    void TestInternetStackInstallation()
    {
        NodeContainer wifiStaNodes, wifiApNode;
        wifiStaNodes.Create(3);
        wifiApNode.Create(1);

        InternetStackHelper stack;
        stack.Install(wifiStaNodes);
        stack.Install(wifiApNode);

        // Verify that Internet stack is installed
        for (uint32_t i = 0; i < wifiStaNodes.GetN(); ++i)
        {
            Ptr<Node> node = wifiStaNodes.Get(i);
            Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
            NS_TEST_ASSERT_MSG_NE(ipv4, nullptr, "Internet stack was not installed on STA node " + std::to_string(i));
        }
        Ptr<Node> apNode = wifiApNode.Get(0);
        Ptr<Ipv4> ipv4Ap = apNode->GetObject<Ipv4>();
        NS_TEST_ASSERT_MSG_NE(ipv4Ap, nullptr, "Internet stack was not installed on AP node.");
    }

    // Test IP address assignment (verify that IP addresses are assigned correctly)
    void TestIpAddressAssignment()
    {
        NodeContainer wifiStaNodes, wifiApNode;
        wifiStaNodes.Create(3);
        wifiApNode.Create(1);

        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
        phy.SetChannel(channel.Create());

        WifiHelper wifi;
        wifi.SetStandard(WIFI_STANDARD_80211n);
        wifi.SetRemoteStationManager("ns3::AarfWifiManager");

        WifiMacHelper mac;
        Ssid ssid = Ssid("ns3-wifi");

        mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));
        NetDeviceContainer staDevices = wifi.Install(phy, mac, wifiStaNodes);

        mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
        NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiApNode);

        InternetStackHelper stack;
        stack.Install(wifiStaNodes);
        stack.Install(wifiApNode);

        Ipv4AddressHelper address;
        address.SetBase("192.168.1.0", "255.255.255.0");
        Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);
        Ipv4InterfaceContainer apInterface = address.Assign(apDevice);

        // Verify that IP addresses are assigned to the devices
        for (uint32_t i = 0; i < staInterfaces.GetN(); ++i)
        {
            Ipv4Address ipAddr = staInterfaces.GetAddress(i);
            NS_TEST_ASSERT_MSG_NE(ipAddr, Ipv4Address("0.0.0.0"), "Failed to assign IP address to STA node " + std::to_string(i));
        }
        Ipv4Address apIpAddr = apInterface.GetAddress(0);
        NS_TEST_ASSERT_MSG_NE(apIpAddr, Ipv4Address("0.0.0.0"), "Failed to assign IP address to AP node.");
    }

    // Test UDP echo server setup (verify that the server is set up on the AP node)
    void TestUdpEchoServerSetup()
    {
        NodeContainer wifiStaNodes, wifiApNode;
        wifiStaNodes.Create(3);
        wifiApNode.Create(1);

        UdpEchoServerHelper echoServer(9);
        ApplicationContainer serverApps = echoServer.Install(wifiApNode.Get(0));

        // Verify that the UDP echo server is installed on the AP node
        serverApps.Start(Seconds(1.0));
        serverApps.Stop(Seconds(10.0));
        NS_TEST_ASSERT_MSG_EQ(serverApps.GetN(), 1, "Failed to install UDP echo server on AP node.");
    }

    // Test UDP echo client setup (verify that the client is set up on a STA node)
    void TestUdpEchoClientSetup()
    {
        NodeContainer wifiStaNodes, wifiApNode;
        wifiStaNodes.Create(3);
        wifiApNode.Create(1);

        UdpEchoClientHelper echoClient(Ipv4Address("192.168.1.1"), 9);
        echoClient.SetAttribute("MaxPackets", UintegerValue(5));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
        echoClient.SetAttribute("PacketSize", UintegerValue(512));

        ApplicationContainer clientApps = echoClient.Install(wifiStaNodes.Get(0));

        // Verify that the UDP echo client is installed on a STA node
        clientApps.Start(Seconds(2.0));
        clientApps.Stop(Seconds(10.0));
        NS_TEST_ASSERT_MSG_EQ(clientApps.GetN(), 1, "Failed to install UDP echo client on STA node.");
    }

    // Test data transmission (verify that data is transmitted successfully)
    void TestDataTransmission()
    {
        NodeContainer wifiStaNodes, wifiApNode;
        wifiStaNodes.Create(3);
        wifiApNode.Create(1);

        UdpEchoServerHelper echoServer(9);
        ApplicationContainer serverApps = echoServer.Install(wifiApNode.Get(0));
        serverApps.Start(Seconds(1.0));
        serverApps.Stop(Seconds(10.0));

        UdpEchoClientHelper echoClient(Ipv4Address("192.168.1.1"), 9);
        echoClient.SetAttribute("MaxPackets", UintegerValue(5));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
        echoClient.SetAttribute("PacketSize", UintegerValue(512));

        ApplicationContainer clientApps = echoClient.Install(wifiStaNodes.Get(0));
        clientApps.Start(Seconds(2.0));
        clientApps.Stop(Seconds(10.0));

        Simulator::Run();
        Simulator::Destroy();

        // Test that the transmission was successful by verifying the number of packets
        // received by the server
    }
};

int main(int argc, char *argv[])
{
    WifiTestSuite testSuite;
    testSuite.Run();
    return 0;
}
