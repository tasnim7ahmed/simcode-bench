#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/aodv-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/test.h"

using namespace ns3;

class ManetAodvExampleTest : public TestCase
{
public:
    ManetAodvExampleTest() : TestCase("Test MANET AODV Example") {}

    virtual void DoRun() override
    {
        TestWifiConfiguration();
        TestMobilityConfiguration();
        TestAodvRouting();
        TestUdpApplications();
        TestSimulation();
    }

private:
    void TestWifiConfiguration()
    {
        // Test WiFi configuration
        NodeContainer nodes;
        nodes.Create(10);

        WifiHelper wifi;
        wifi.SetStandard(WIFI_STANDARD_80211b);
        WifiMacHelper mac;
        mac.SetType("ns3::AdhocWifiMac");

        YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        phy.SetChannel(channel.Create());

        NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

        // Check if devices were installed correctly
        NS_TEST_ASSERT_MSG_EQ(devices.GetN(), 10, "WiFi devices were not installed correctly");
    }

    void TestMobilityConfiguration()
    {
        // Test mobility configuration for RandomWaypoint model
        NodeContainer nodes;
        nodes.Create(10);

        MobilityHelper mobility;
        mobility.SetMobilityModel("ns3::RandomWaypointMobilityModel",
                                  "Speed", StringValue("ns3::ConstantRandomVariable[Constant=5.0]"),
                                  "Pause", StringValue("ns3::ConstantRandomVariable[Constant=2.0]"),
                                  "PositionAllocator", StringValue("ns3::RandomRectanglePositionAllocator[MinX=0.0|MinY=0.0|MaxX=500.0|MaxY=500.0]"));
        mobility.Install(nodes);

        // Check if mobility model is set correctly
        Ptr<RandomWaypointMobilityModel> mobilityModel = nodes.Get(0)->GetObject<RandomWaypointMobilityModel>();
        NS_TEST_ASSERT_MSG_EQ(mobilityModel != nullptr, true, "Mobility model is not configured correctly");
    }

    void TestAodvRouting()
    {
        // Test AODV routing protocol installation
        NodeContainer nodes;
        nodes.Create(10);

        AodvHelper aodv;
        InternetStackHelper internet;
        internet.SetRoutingHelper(aodv);
        internet.Install(nodes);

        // Check if AODV routing is correctly installed by verifying routing table
        Ptr<AodvRoutingProtocol> routingProtocol = nodes.Get(0)->GetObject<AodvRoutingProtocol>();
        NS_TEST_ASSERT_MSG_EQ(routingProtocol != nullptr, true, "AODV routing protocol not installed correctly");
    }

    void TestUdpApplications()
    {
        // Test UDP server and client setup
        NodeContainer nodes;
        nodes.Create(10);

        uint16_t port = 4000;

        // Set up UDP server on last node
        UdpServerHelper udpServer(port);
        ApplicationContainer serverApp = udpServer.Install(nodes.Get(9));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(20.0));

        // Set up UDP client on first node
        UdpClientHelper udpClient(nodes.Get(9)->GetObject<Ipv4>()->GetAddress(1, 0).GetAddress(), port);
        udpClient.SetAttribute("MaxPackets", UintegerValue(50));
        udpClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
        udpClient.SetAttribute("PacketSize", UintegerValue(1024));

        ApplicationContainer clientApp = udpClient.Install(nodes.Get(0));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(20.0));

        // Check if server and client applications are correctly installed
        NS_TEST_ASSERT_MSG_EQ(serverApp.GetN(), 1, "UDP server application not installed correctly");
        NS_TEST_ASSERT_MSG_EQ(clientApp.GetN(), 1, "UDP client application not installed correctly");
    }

    void TestSimulation()
    {
        // Test that the simulation runs and completes correctly
        NodeContainer nodes;
        nodes.Create(10);

        WifiHelper wifi;
        wifi.SetStandard(WIFI_STANDARD_80211b);
        WifiMacHelper mac;
        mac.SetType("ns3::AdhocWifiMac");

        YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        phy.SetChannel(channel.Create());

        NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

        MobilityHelper mobility;
        mobility.SetMobilityModel("ns3::RandomWaypointMobilityModel",
                                  "Speed", StringValue("ns3::ConstantRandomVariable[Constant=5.0]"),
                                  "Pause", StringValue("ns3::ConstantRandomVariable[Constant=2.0]"),
                                  "PositionAllocator", StringValue("ns3::RandomRectanglePositionAllocator[MinX=0.0|MinY=0.0|MaxX=500.0|MaxY=500.0]"));
        mobility.Install(nodes);

        AodvHelper aodv;
        InternetStackHelper internet;
        internet.SetRoutingHelper(aodv);
        internet.Install(nodes);

        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.1.1.0", "255.255.255.0");
        ipv4.Assign(devices);

        uint16_t port = 4000;
        UdpServerHelper udpServer(port);
        ApplicationContainer serverApp = udpServer.Install(nodes.Get(9));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(20.0));

        UdpClientHelper udpClient(nodes.Get(9)->GetObject<Ipv4>()->GetAddress(1, 0).GetAddress(), port);
        udpClient.SetAttribute("MaxPackets", UintegerValue(50));
        udpClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
        udpClient.SetAttribute("PacketSize", UintegerValue(1024));

        ApplicationContainer clientApp = udpClient.Install(nodes.Get(0));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(20.0));

        Simulator::Run();
        Simulator::Destroy();

        // Check if simulation ran without errors
        NS_TEST_ASSERT_MSG_EQ(Simulator::GetContext()->GetTotalSimTime().GetSeconds() > 0, true, "Simulation did not run correctly");
    }
};

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);
    LogComponentEnable("ManetAodvExample", LOG_LEVEL_INFO);

    // Run the unit tests
    Ptr<ManetAodvExampleTest> test = CreateObject<ManetAodvExampleTest>();
    test->Run();

    return 0;
}
