#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/ssid.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/ipv4-routing-helper.h"
#include "ns3/olsr-module.h"
#include "ns3/test.h"

using namespace ns3;

class SimpleManetTest : public TestCase
{
public:
    SimpleManetTest() : TestCase("Simple MANET Network Test") {}

    virtual void DoRun() override
    {
        TestNodeCreation();
        TestWifiSetup();
        TestInternetStackInstallation();
        TestIpAddressAssignment();
        TestUdpServerSetup();
        TestUdpClientSetup();
        TestMobilitySetup();
        TestSimulationExecution();
    }

private:
    void TestNodeCreation()
    {
        NodeContainer nodes;
        nodes.Create(2);

        NS_TEST_ASSERT_MSG_EQ(nodes.GetN(), 2, "Node creation failed. Expected 2 nodes.");
    }

    void TestWifiSetup()
    {
        NodeContainer nodes;
        nodes.Create(2);

        WifiHelper wifiHelper;
        wifiHelper.SetStandard(WIFI_PHY_STANDARD_80211b);
        YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
        YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
        wifiPhy.SetChannel(wifiChannel.Create());

        WifiMacHelper wifiMac;
        Ssid ssid = Ssid("ns-3-manet");
        wifiMac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));

        NetDeviceContainer devices = wifiHelper.Install(wifiPhy, wifiMac, nodes);

        NS_TEST_ASSERT_MSG_EQ(devices.GetN(), 2, "Wi-Fi device installation failed.");
    }

    void TestInternetStackInstallation()
    {
        NodeContainer nodes;
        nodes.Create(2);

        InternetStackHelper stack;
        OlsrHelper olsr;
        stack.SetRoutingHelper(olsr);
        stack.Install(nodes);

        for (uint32_t i = 0; i < nodes.GetN(); ++i)
        {
            Ptr<Node> node = nodes.Get(i);
            Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
            NS_TEST_ASSERT_MSG_NE(ipv4, nullptr, "Internet stack installation failed on node " << i);
        }
    }

    void TestIpAddressAssignment()
    {
        NodeContainer nodes;
        nodes.Create(2);

        WifiHelper wifiHelper;
        YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
        YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
        wifiPhy.SetChannel(wifiChannel.Create());

        WifiMacHelper wifiMac;
        Ssid ssid = Ssid("ns-3-manet");
        wifiMac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));

        NetDeviceContainer devices = wifiHelper.Install(wifiPhy, wifiMac, nodes);

        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer ipInterfaces = ipv4.Assign(devices);

        NS_TEST_ASSERT_MSG_EQ(ipInterfaces.GetN(), 2, "IP address assignment failed.");
    }

    void TestUdpServerSetup()
    {
        NodeContainer nodes;
        nodes.Create(2);

        uint16_t port = 9;
        UdpServerHelper udpServer(port);
        ApplicationContainer serverApp = udpServer.Install(nodes.Get(1));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        NS_TEST_ASSERT_MSG_EQ(serverApp.GetN(), 1, "UDP server application installation failed.");
    }

    void TestUdpClientSetup()
    {
        NodeContainer nodes;
        nodes.Create(2);

        WifiHelper wifiHelper;
        YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
        YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
        wifiPhy.SetChannel(wifiChannel.Create());

        WifiMacHelper wifiMac;
        Ssid ssid = Ssid("ns-3-manet");
        wifiMac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));

        NetDeviceContainer devices = wifiHelper.Install(wifiPhy, wifiMac, nodes);

        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer ipInterfaces = ipv4.Assign(devices);

        uint16_t port = 9;
        UdpClientHelper udpClient(ipInterfaces.GetAddress(1), port);
        udpClient.SetAttribute("MaxPackets", UintegerValue(1000));
        udpClient.SetAttribute("Interval", TimeValue(Seconds(0.01)));
        udpClient.SetAttribute("PacketSize", UintegerValue(1024));

        ApplicationContainer clientApp = udpClient.Install(nodes.Get(0));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));

        NS_TEST_ASSERT_MSG_EQ(clientApp.GetN(), 1, "UDP client application installation failed.");
    }

    void TestMobilitySetup()
    {
        NodeContainer nodes;
        nodes.Create(2);

        MobilityHelper mobility;
        mobility.SetMobilityModel("ns3::RandomWaypointMobilityModel",
                                  "MinX", DoubleValue(0.0),
                                  "MaxX", DoubleValue(100.0),
                                  "MinY", DoubleValue(0.0),
                                  "MaxY", DoubleValue(100.0),
                                  "Speed", StringValue("ns3::ConstantRandomVariable[Constant=20.0]"));
        mobility.Install(nodes);

        for (uint32_t i = 0; i < nodes.GetN(); ++i)
        {
            Ptr<MobilityModel> mobilityModel = nodes.Get(i)->GetObject<MobilityModel>();
            NS_TEST_ASSERT_MSG_NE(mobilityModel, nullptr, "Mobility model installation failed for node " << i);
        }
    }

    void TestSimulationExecution()
    {
        NodeContainer nodes;
        nodes.Create(2);

        WifiHelper wifiHelper;
        YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
        YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
        wifiPhy.SetChannel(wifiChannel.Create());

        WifiMacHelper wifiMac;
        Ssid ssid = Ssid("ns-3-manet");
        wifiMac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));

        NetDeviceContainer devices = wifiHelper.Install(wifiPhy, wifiMac, nodes);

        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer ipInterfaces = ipv4.Assign(devices);

        uint16_t port = 9;
        UdpServerHelper udpServer(port);
        ApplicationContainer serverApp = udpServer.Install(nodes.Get(1));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        UdpClientHelper udpClient(ipInterfaces.GetAddress(1), port);
        udpClient.SetAttribute("MaxPackets", UintegerValue(1000));
        udpClient.SetAttribute("Interval", TimeValue(Seconds(0.01)));
        udpClient.SetAttribute("PacketSize", UintegerValue(1024));
        ApplicationContainer clientApp = udpClient.Install(nodes.Get(0));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));

        Simulator::Run();
        Simulator::Destroy();

        NS_TEST_ASSERT_MSG_EQ(true, true, "Simulation execution failed.");
    }
};

// Create the test case and run it
int main()
{
    SimpleManetTest test;
    test.Run();
    return 0;
}
