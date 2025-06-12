#include "ns3/test.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

class WifiTestSuite : public TestCase
{
public:
    WifiTestSuite() : TestCase("Test Wi-Fi network setup with UDP applications") {}
    virtual ~WifiTestSuite() {}

    void DoRun() override
    {
        TestNodeCreation();
        TestWifiModuleSetup();
        TestInternetStackInstallation();
        TestIpv4AddressAssignment();
        TestUdpServerSetup();
        TestUdpClientSetup();
        TestDataTransmission();
        TestMobilityModelSetup();
    }

private:
    // Test node creation (verify that AP and STA nodes are created)
    void TestNodeCreation()
    {
        NodeContainer apNode, staNodes;
        apNode.Create(1);
        staNodes.Create(3);

        // Verify that 1 AP node and 3 STA nodes are created
        NS_TEST_ASSERT_MSG_EQ(apNode.GetN(), 1, "Failed to create AP node.");
        NS_TEST_ASSERT_MSG_EQ(staNodes.GetN(), 3, "Failed to create STA nodes.");
    }

    // Test Wi-Fi module setup (verify that Wi-Fi devices are installed correctly)
    void TestWifiModuleSetup()
    {
        NodeContainer apNode, staNodes;
        apNode.Create(1);
        staNodes.Create(3);

        WifiHelper wifiHelper;
        wifiHelper.SetStandard(WIFI_PHY_STANDARD_80211n_5GHZ);

        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
        phy.SetChannel(channel.Create());

        WifiMacHelper mac;
        Ssid ssid = Ssid("ns3-wifi");
        mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));

        // Install Wi-Fi devices on the AP and STAs
        NetDeviceContainer apDevice, staDevices;
        apDevice = wifiHelper.Install(phy, mac, apNode);
        mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
        staDevices = wifiHelper.Install(phy, mac, staNodes);

        // Verify that Wi-Fi devices are installed on the AP and STA nodes
        NS_TEST_ASSERT_MSG_EQ(apDevice.GetN(), 1, "Failed to install Wi-Fi device on AP node.");
        NS_TEST_ASSERT_MSG_EQ(staDevices.GetN(), 3, "Failed to install Wi-Fi devices on STA nodes.");
    }

    // Test Internet stack installation (verify that the stack is installed correctly)
    void TestInternetStackInstallation()
    {
        NodeContainer apNode, staNodes;
        apNode.Create(1);
        staNodes.Create(3);

        InternetStackHelper internet;
        internet.Install(apNode);
        internet.Install(staNodes);

        // Verify that the Internet stack is installed on both AP and STA nodes
        Ptr<Ipv4> apIpv4 = apNode.Get(0)->GetObject<Ipv4>();
        Ptr<Ipv4> staIpv4 = staNodes.Get(0)->GetObject<Ipv4>();

        NS_TEST_ASSERT_MSG_NE(apIpv4, nullptr, "Failed to install Internet stack on AP node.");
        NS_TEST_ASSERT_MSG_NE(staIpv4, nullptr, "Failed to install Internet stack on STA node.");
    }

    // Test IPv4 address assignment (verify that IP addresses are assigned correctly)
    void TestIpv4AddressAssignment()
    {
        NodeContainer apNode, staNodes;
        apNode.Create(1);
        staNodes.Create(3);

        WifiHelper wifiHelper;
        wifiHelper.SetStandard(WIFI_PHY_STANDARD_80211n_5GHZ);

        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
        phy.SetChannel(channel.Create());

        WifiMacHelper mac;
        Ssid ssid = Ssid("ns3-wifi");
        mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));

        // Install Wi-Fi devices on the AP and STAs
        NetDeviceContainer apDevice, staDevices;
        apDevice = wifiHelper.Install(phy, mac, apNode);
        mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
        staDevices = wifiHelper.Install(phy, mac, staNodes);

        // Install IP stack for routing
        InternetStackHelper internet;
        internet.Install(apNode);
        internet.Install(staNodes);

        // Assign IP addresses to the devices
        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer apIpIface, staIpIface;
        apIpIface = ipv4.Assign(apDevice);
        staIpIface = ipv4.Assign(staDevices);

        // Verify that IP addresses are assigned to AP and STA nodes
        NS_TEST_ASSERT_MSG_NE(apIpIface.GetAddress(0), Ipv4Address("0.0.0.0"), "Failed to assign IP address to AP.");
        for (uint32_t i = 0; i < 3; ++i)
        {
            NS_TEST_ASSERT_MSG_NE(staIpIface.GetAddress(i), Ipv4Address("0.0.0.0"), "Failed to assign IP address to STA.");
        }
    }

    // Test UDP server setup (verify that the UDP server is installed on the AP)
    void TestUdpServerSetup()
    {
        NodeContainer apNode;
        apNode.Create(1);

        uint16_t port = 9;
        UdpServerHelper udpServer(port);
        ApplicationContainer serverApp = udpServer.Install(apNode.Get(0));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        // Verify that the UDP server is installed on the AP
        NS_TEST_ASSERT_MSG_EQ(serverApp.GetN(), 1, "Failed to install UDP server on AP node.");
    }

    // Test UDP client setup (verify that the UDP client is installed on STA nodes)
    void TestUdpClientSetup()
    {
        NodeContainer staNodes;
        staNodes.Create(3);

        Ipv4AddressHelper ipv4;
        Ipv4InterfaceContainer apIpIface;
        apIpIface.SetBase("10.1.1.0", "255.255.255.0");

        uint16_t port = 9;
        UdpClientHelper udpClient(apIpIface.GetAddress(0), port);
        udpClient.SetAttribute("MaxPackets", UintegerValue(5));
        udpClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
        udpClient.SetAttribute("PacketSize", UintegerValue(512));

        ApplicationContainer clientApps;
        for (uint32_t i = 0; i < 3; ++i)
        {
            clientApps.Add(udpClient.Install(staNodes.Get(i)));
        }
        clientApps.Start(Seconds(2.0));
        clientApps.Stop(Seconds(10.0));

        // Verify that the UDP clients are installed on STA nodes
        NS_TEST_ASSERT_MSG_EQ(clientApps.GetN(), 3, "Failed to install UDP clients on STA nodes.");
    }

    // Test data transmission (verify that data can be transmitted between the UDP clients and server)
    void TestDataTransmission()
    {
        NodeContainer apNode, staNodes;
        apNode.Create(1);
        staNodes.Create(3);

        WifiHelper wifiHelper;
        wifiHelper.SetStandard(WIFI_PHY_STANDARD_80211n_5GHZ);

        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
        phy.SetChannel(channel.Create());

        WifiMacHelper mac;
        Ssid ssid = Ssid("ns3-wifi");
        mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));

        // Install Wi-Fi devices on the AP and STAs
        NetDeviceContainer apDevice, staDevices;
        apDevice = wifiHelper.Install(phy, mac, apNode);
        mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
        staDevices = wifiHelper.Install(phy, mac, staNodes);

        // Install IP stack for routing
        InternetStackHelper internet;
        internet.Install(apNode);
        internet.Install(staNodes);

        // Assign IP addresses to the devices
        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer apIpIface, staIpIface;
        apIpIface = ipv4.Assign(apDevice);
        staIpIface = ipv4.Assign(staDevices);

        // Set up the UDP server on AP
        uint16_t port = 9;
        UdpServerHelper udpServer(port);
        ApplicationContainer serverApp = udpServer.Install(apNode.Get(0));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        // Set up UDP clients on STA nodes
        UdpClientHelper udpClient(apIpIface.GetAddress(0), port);
        udpClient.SetAttribute("MaxPackets", UintegerValue(5));
        udpClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
        udpClient.SetAttribute("PacketSize", UintegerValue(512));
        ApplicationContainer clientApps;
        for (uint32_t i = 0; i < 3; ++i)
        {
            clientApps.Add(udpClient.Install(staNodes.Get(i)));
        }
        clientApps.Start(Seconds(2.0));
        clientApps.Stop(Seconds(10.0));

        // Run the simulation to check transmission
        Simulator::Run();
        Simulator::Destroy();

        // Further packet inspection or callback functions can be added to verify data reception
    }

    // Test mobility model setup (verify that mobility models are set correctly for AP and STA nodes)
    void TestMobilityModelSetup()
    {
        NodeContainer apNode, staNodes;
        apNode.Create(1);
        staNodes.Create(3);

        MobilityHelper mobility;

        // Set mobility for AP (static position)
        mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                      "MinX", DoubleValue(0.0),
                                      "MinY", DoubleValue(0.0),
                                      "DeltaX", DoubleValue(10.0),
                                      "DeltaY", DoubleValue(10.0),
                                      "GridWidth", UintegerValue(3),
                                      "LayoutType", StringValue("RowFirst"));
        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        mobility.Install(apNode);

        // Set mobility for STA nodes (random walk)
        mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                      "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"),
                                      "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"));
        mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                                  "Bounds", RectangleValue(Rectangle(0, 50, 0, 50)));
        mobility.Install(staNodes);

        // Verify that the mobility models are installed on both AP and STA nodes
        Ptr<ConstantPositionMobilityModel> apMobility = apNode.Get(0)->GetObject<ConstantPositionMobilityModel>();
        Ptr<RandomWalk2dMobilityModel> staMobility = staNodes.Get(0)->GetObject<RandomWalk2dMobilityModel>();

        NS_TEST_ASSERT_MSG_NE(apMobility, nullptr, "Failed to install mobility model on AP.");
        NS_TEST_ASSERT_MSG_NE(staMobility, nullptr, "Failed to install mobility model on STA.");
    }
};

static WifiTestSuite suite;

