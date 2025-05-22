#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ssid.h"
#include "ns3/mobility-module.h"
#include "ns3/test.h"

using namespace ns3;

// Unit Test Suite for Wi-Fi Example
class WifiSimpleExampleTest : public TestCase
{
public:
    WifiSimpleExampleTest() : TestCase("Test for WifiSimpleExample") {}
    virtual void DoRun()
    {
        TestNodeCreation();
        TestWifiDeviceInstallation();
        TestInternetStackInstallation();
        TestIpAddressAssignment();
        TestMobilityModelInstallation();
        TestUdpServerSetup();
        TestUdpClientSetup();
        TestSimulationExecution();
    }

private:
    uint32_t nStaNodes = 1;
    uint32_t nApNodes = 1;
    NodeContainer wifiStaNodes;
    NodeContainer wifiApNode;
    Ipv4InterfaceContainer staInterfaces;
    Ipv4InterfaceContainer apInterfaces;

    // Test for node creation
    void TestNodeCreation()
    {
        wifiStaNodes.Create(nStaNodes);
        wifiApNode.Create(nApNodes);

        // Verify nodes are created
        NS_TEST_ASSERT_MSG_EQ(wifiStaNodes.GetN(), nStaNodes, "Failed to create STA nodes");
        NS_TEST_ASSERT_MSG_EQ(wifiApNode.GetN(), nApNodes, "Failed to create AP node");
    }

    // Test for Wi-Fi device installation
    void TestWifiDeviceInstallation()
    {
        WifiHelper wifi;
        wifi.SetRemoteStationManager("ns3::AarfWifiManager");

        // Set up the physical layer
        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
        phy.SetChannel(channel.Create());

        // Set up MAC layer
        WifiMacHelper mac;
        Ssid ssid = Ssid("ns3-ssid");
        mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));

        // Install Wi-Fi devices on STA node
        NetDeviceContainer staDevice = wifi.Install(phy, mac, wifiStaNodes);
        mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));

        // Install Wi-Fi devices on AP node
        NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiApNode);

        // Verify that Wi-Fi devices were installed
        NS_TEST_ASSERT_MSG_EQ(staDevice.GetN(), nStaNodes, "Failed to install Wi-Fi devices on STA node");
        NS_TEST_ASSERT_MSG_EQ(apDevice.GetN(), nApNodes, "Failed to install Wi-Fi devices on AP node");
    }

    // Test for Internet stack installation
    void TestInternetStackInstallation()
    {
        InternetStackHelper stack;
        stack.Install(wifiStaNodes);
        stack.Install(wifiApNode);

        // Verify that Internet stack is installed correctly on STA and AP
        for (uint32_t i = 0; i < wifiStaNodes.GetN(); ++i)
        {
            Ptr<Ipv4> ipv4 = wifiStaNodes.Get(i)->GetObject<Ipv4>();
            NS_TEST_ASSERT_MSG_NE(ipv4, nullptr, "Failed to install Internet stack on STA node");
        }
        for (uint32_t i = 0; i < wifiApNode.GetN(); ++i)
        {
            Ptr<Ipv4> ipv4 = wifiApNode.Get(i)->GetObject<Ipv4>();
            NS_TEST_ASSERT_MSG_NE(ipv4, nullptr, "Failed to install Internet stack on AP node");
        }
    }

    // Test for IP address assignment
    void TestIpAddressAssignment()
    {
        Ipv4AddressHelper address;
        address.SetBase("10.1.1.0", "255.255.255.0");

        // Assign IP addresses to nodes
        NetDeviceContainer staDevice = wifi.Install(phy, mac, wifiStaNodes);
        NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiApNode);

        staInterfaces = address.Assign(staDevice);
        apInterfaces = address.Assign(apDevice);

        // Verify that IP addresses were assigned
        Ipv4Address staIp = staInterfaces.GetAddress(0);
        Ipv4Address apIp = apInterfaces.GetAddress(0);

        NS_TEST_ASSERT_MSG_NE(staIp, Ipv4Address("0.0.0.0"), "Invalid IP address assigned to STA");
        NS_TEST_ASSERT_MSG_NE(apIp, Ipv4Address("0.0.0.0"), "Invalid IP address assigned to AP");
    }

    // Test for mobility model installation
    void TestMobilityModelInstallation()
    {
        MobilityHelper mobility;
        mobility.SetPositionAllocator("ns3::GridPositionAllocator", "MinX", DoubleValue(0.0), "MinY", DoubleValue(0.0),
                                      "DeltaX", DoubleValue(5.0), "DeltaY", DoubleValue(5.0), "GridWidth", UintegerValue(1),
                                      "LayoutType", StringValue("RowFirst"));
        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");

        // Install mobility model on STA and AP nodes
        mobility.Install(wifiStaNodes);
        mobility.Install(wifiApNode);

        // Verify that the mobility model is installed
        Ptr<ConstantPositionMobilityModel> mobilityModelSta = wifiStaNodes.Get(0)->GetObject<ConstantPositionMobilityModel>();
        Ptr<ConstantPositionMobilityModel> mobilityModelAp = wifiApNode.Get(0)->GetObject<ConstantPositionMobilityModel>();

        NS_TEST_ASSERT_MSG_NE(mobilityModelSta, nullptr, "Failed to install mobility model on STA node");
        NS_TEST_ASSERT_MSG_NE(mobilityModelAp, nullptr, "Failed to install mobility model on AP node");
    }

    // Test for UDP server setup on STA
    void TestUdpServerSetup()
    {
        UdpServerHelper server(9);
        ApplicationContainer serverApp = server.Install(wifiStaNodes.Get(0));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        // Verify that the UDP server is installed correctly
        NS_TEST_ASSERT_MSG_EQ(serverApp.GetN(), 1, "Failed to install UDP server on STA node");
    }

    // Test for UDP client setup on AP
    void TestUdpClientSetup()
    {
        UdpClientHelper client(staInterfaces.GetAddress(0), 9);
        client.SetAttribute("MaxPackets", UintegerValue(1000));
        client.SetAttribute("Interval", TimeValue(MilliSeconds(20)));
        client.SetAttribute("PacketSize", UintegerValue(512));

        ApplicationContainer clientApp = client.Install(wifiApNode.Get(0));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));

        // Verify that the UDP client is installed correctly
        NS_TEST_ASSERT_MSG_EQ(clientApp.GetN(), 1, "Failed to install UDP client on AP node");
    }

    // Test for simulation execution
    void TestSimulationExecution()
    {
        Simulator::Run();
        Simulator::Destroy();

        // Verify simulation ran and was destroyed without errors
        NS_TEST_ASSERT_MSG_EQ(Simulator::GetContext(), 0, "Simulation encountered an error during execution");
    }
};

// Register the test
static WifiSimpleExampleTest wifiSimpleExampleTestInstance;
