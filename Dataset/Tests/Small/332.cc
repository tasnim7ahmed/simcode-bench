#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/test.h"

using namespace ns3;

// Unit Test Suite for WiFi Ad-Hoc UDP Setup
class WifiAdhocTest : public TestCase
{
public:
    WifiAdhocTest() : TestCase("Test for WiFi Ad-Hoc UDP Setup") {}
    virtual void DoRun()
    {
        TestNodeCreation();
        TestMobilityModelSetup();
        TestWiFiSetup();
        TestInternetStackInstallation();
        TestIpAddressAssignment();
        TestUdpServerSetup();
        TestUdpClientSetup();
        TestSimulationExecution();
    }

private:
    NodeContainer nodes;
    NetDeviceContainer devices;
    Ipv4InterfaceContainer interfaces;
    uint16_t udpPort = 9;

    void TestNodeCreation()
    {
        nodes.Create(2);
        NS_TEST_ASSERT_MSG_EQ(nodes.GetN(), 2, "Failed to create 2 nodes (Sender and Receiver)");
    }

    void TestMobilityModelSetup()
    {
        MobilityHelper mobility;
        mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                      "MinX", DoubleValue(0.0),
                                      "MinY", DoubleValue(0.0),
                                      "DeltaX", DoubleValue(10.0),
                                      "DeltaY", DoubleValue(10.0),
                                      "GridWidth", UintegerValue(1),
                                      "LayoutType", StringValue("RowFirst"));
        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        mobility.Install(nodes);

        Ptr<MobilityModel> mobility0 = nodes.Get(0)->GetObject<MobilityModel>();
        Ptr<MobilityModel> mobility1 = nodes.Get(1)->GetObject<MobilityModel>();

        NS_TEST_ASSERT_MSG_NE(mobility0, nullptr, "Mobility model not installed on Node 0");
        NS_TEST_ASSERT_MSG_NE(mobility1, nullptr, "Mobility model not installed on Node 1");
    }

    void TestWiFiSetup()
    {
        WifiHelper wifi;
        wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

        YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
        wifiPhy.SetChannel(YansWifiChannelHelper::Default().Create());

        WifiMacHelper wifiMac;
        wifiMac.SetType("ns3::AdhocWifiMac");

        devices = wifi.Install(wifiPhy, wifiMac, nodes);

        NS_TEST_ASSERT_MSG_GT(devices.GetN(), 1, "WiFi devices not installed on nodes in Ad-Hoc mode");
    }

    void TestInternetStackInstallation()
    {
        InternetStackHelper stack;
        stack.Install(nodes);

        for (uint32_t i = 0; i < nodes.GetN(); ++i)
        {
            Ptr<Ipv4> ipv4 = nodes.Get(i)->GetObject<Ipv4>();
            NS_TEST_ASSERT_MSG_NE(ipv4, nullptr, "Internet stack not installed on Node " + std::to_string(i));
        }
    }

    void TestIpAddressAssignment()
    {
        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.1.1.0", "255.255.255.0");
        interfaces = ipv4.Assign(devices);

        for (uint32_t i = 0; i < interfaces.GetN(); ++i)
        {
            NS_TEST_ASSERT_MSG_NE(interfaces.GetAddress(i), Ipv4Address("0.0.0.0"), "Invalid IP address assigned");
        }
    }

    void TestUdpServerSetup()
    {
        UdpServerHelper udpServer(udpPort);
        ApplicationContainer serverApp = udpServer.Install(nodes.Get(1));

        NS_TEST_ASSERT_MSG_GT(serverApp.GetN(), 0, "UDP server application not installed on Node 1 (Receiver)");
    }

    void TestUdpClientSetup()
    {
        UdpClientHelper udpClient(interfaces.GetAddress(1), udpPort); // Send to Node 1
        udpClient.SetAttribute("MaxPackets", UintegerValue(10));     // Number of packets to send
        udpClient.SetAttribute("Interval", TimeValue(Seconds(1.0))); // Interval between packets
        udpClient.SetAttribute("PacketSize", UintegerValue(1024));   // Packet size in bytes

        ApplicationContainer clientApp = udpClient.Install(nodes.Get(0)); // Install on Node 0 (Sender)

        NS_TEST_ASSERT_MSG_GT(clientApp.GetN(), 0, "UDP client application not installed on Node 0 (Sender)");
    }

    void TestSimulationExecution()
    {
        Simulator::Run();
        Simulator::Destroy();
        NS_TEST_ASSERT_MSG_EQ(Simulator::GetContext(), 0, "Simulation encountered an error");
    }
};

// Register the test
static WifiAdhocTest wifiAdhocTestInstance;
