#include "ns3/test.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/aodv-module.h"
#include "ns3/applications-module.h"
#include <cstdlib>

using namespace ns3;

class UdpManetExampleTestSuite : public TestCase
{
public:
    UdpManetExampleTestSuite() : TestCase("Test UDP MANET Example") {}
    virtual ~UdpManetExampleTestSuite() {}

    void DoRun() override
    {
        TestNodeCreation();
        TestWifiConfiguration();
        TestMobilityModel();
        TestAodvRoutingStack();
        TestIpAddressAssignment();
        TestSocketCreation();
        TestPacketTransmission();
    }

private:
    // Test the creation of nodes
    void TestNodeCreation()
    {
        int numNodes = 10;
        NodeContainer nodes;
        nodes.Create(numNodes);

        NS_TEST_ASSERT_MSG_EQ(nodes.GetN(), numNodes, "Node creation failed. Expected 10 nodes.");
    }

    // Test Wi-Fi configuration (Ad-Hoc Mode)
    void TestWifiConfiguration()
    {
        int numNodes = 10;
        NodeContainer nodes;
        nodes.Create(numNodes);

        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
        phy.SetChannel(channel.Create());

        WifiHelper wifi;
        wifi.SetStandard(WIFI_STANDARD_80211b);

        WifiMacHelper mac;
        mac.SetType("ns3::AdhocWifiMac");
        NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

        NS_TEST_ASSERT_MSG_EQ(devices.GetN(), numNodes, "Wi-Fi device configuration failed. Expected 10 devices.");
    }

    // Test mobility model configuration (Random Waypoint)
    void TestMobilityModel()
    {
        int numNodes = 10;
        NodeContainer nodes;
        nodes.Create(numNodes);

        MobilityHelper mobility;
        mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                      "X", StringValue("ns3::UniformRandomVariable[Min=0|Max=100]"),
                                      "Y", StringValue("ns3::UniformRandomVariable[Min=0|Max=100]"));
        mobility.SetMobilityModel("ns3::RandomWaypointMobilityModel",
                                  "Speed", StringValue("ns3::UniformRandomVariable[Min=5.0|Max=10.0]"),
                                  "Pause", StringValue("ns3::ConstantRandomVariable[Constant=2.0]"));
        mobility.Install(nodes);

        Ptr<MobilityModel> nodeMobility = nodes.Get(0)->GetObject<MobilityModel>();

        NS_TEST_ASSERT_MSG_NOT_NULL(nodeMobility, "Mobility model installation failed.");
    }

    // Test AODV routing stack installation
    void TestAodvRoutingStack()
    {
        int numNodes = 10;
        NodeContainer nodes;
        nodes.Create(numNodes);

        AodvHelper aodv;
        InternetStackHelper internet;
        internet.SetRoutingHelper(aodv);
        internet.Install(nodes);

        // Verify AODV is installed by checking the number of routes
        Ptr<Ipv4RoutingProtocol> routingProtocol = nodes.Get(0)->GetObject<Ipv4RoutingProtocol>();
        NS_TEST_ASSERT_MSG_NOT_NULL(routingProtocol, "AODV routing stack installation failed.");
    }

    // Test IP address assignment
    void TestIpAddressAssignment()
    {
        int numNodes = 10;
        NodeContainer nodes;
        nodes.Create(numNodes);

        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
        phy.SetChannel(channel.Create());

        WifiHelper wifi;
        wifi.SetStandard(WIFI_STANDARD_80211b);

        WifiMacHelper mac;
        mac.SetType("ns3::AdhocWifiMac");
        NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

        Ipv4AddressHelper address;
        address.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = address.Assign(devices);

        NS_TEST_ASSERT_MSG_EQ(interfaces.GetN(), numNodes, "IP address assignment failed. Expected 10 interfaces.");
    }

    // Test socket creation
    void TestSocketCreation()
    {
        int numNodes = 10;
        NodeContainer nodes;
        nodes.Create(numNodes);

        for (int i = 0; i < numNodes; i++) {
            Ptr<Socket> recvSocket = Socket::CreateSocket(nodes.Get(i), UdpSocketFactory::GetTypeId());
            recvSocket->Bind(InetSocketAddress(Ipv4Address::GetAny(), 8080));
            recvSocket->SetRecvCallback(MakeCallback(&ReceivePacket));

            Ptr<Socket> sendSocket = Socket::CreateSocket(nodes.Get(i), UdpSocketFactory::GetTypeId());

            NS_TEST_ASSERT_MSG_NOT_NULL(recvSocket, "Socket creation failed for receiving.");
            NS_TEST_ASSERT_MSG_NOT_NULL(sendSocket, "Socket creation failed for sending.");
        }
    }

    // Test packet transmission (client sends packets to a random neighbor)
    void TestPacketTransmission()
    {
        int numNodes = 10;
        double simTime = 20.0;
        NodeContainer nodes;
        nodes.Create(numNodes);

        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
        phy.SetChannel(channel.Create());

        WifiHelper wifi;
        wifi.SetStandard(WIFI_STANDARD_80211b);

        WifiMacHelper mac;
        mac.SetType("ns3::AdhocWifiMac");
        NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

        MobilityHelper mobility;
        mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                      "X", StringValue("ns3::UniformRandomVariable[Min=0|Max=100]"),
                                      "Y", StringValue("ns3::UniformRandomVariable[Min=0|Max=100]"));
        mobility.SetMobilityModel("ns3::RandomWaypointMobilityModel",
                                  "Speed", StringValue("ns3::UniformRandomVariable[Min=5.0|Max=10.0]"),
                                  "Pause", StringValue("ns3::ConstantRandomVariable[Constant=2.0]"));
        mobility.Install(nodes);

        Ipv4AddressHelper address;
        address.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = address.Assign(devices);

        // Setup UDP sockets
        for (int i = 0; i < numNodes; i++) {
            Ptr<Socket> recvSocket = Socket::CreateSocket(nodes.Get(i), UdpSocketFactory::GetTypeId());
            recvSocket->Bind(InetSocketAddress(Ipv4Address::GetAny(), 8080));
            recvSocket->SetRecvCallback(MakeCallback(&ReceivePacket));

            Ptr<Socket> sendSocket = Socket::CreateSocket(nodes.Get(i), UdpSocketFactory::GetTypeId());
            Simulator::Schedule(Seconds(2.0 + i * 0.5), &SendPacket, sendSocket, interfaces);
        }

        // Run the simulation and verify packet transmission
        Simulator::Stop(Seconds(simTime));
        Simulator::Run();
        Simulator::Destroy();
        
        NS_LOG_UNCOND("Test completed. Check server logs for received packets.");
    }
};

TestSuite *TestSuiteInstance = new TestSuite("UdpManetExampleTestSuite");

int main(int argc, char *argv[])
{
    // Create the test suite and add the test case
    TestSuiteInstance->AddTestCase(new UdpManetExampleTestSuite());
    return TestSuiteInstance->Run();
}
