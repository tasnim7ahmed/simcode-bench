#include "ns3/test.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/aodv-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

class AodvTestSuite : public TestCase
{
public:
    AodvTestSuite() : TestCase("Test AODV Routing with UDP Applications") {}
    virtual ~AodvTestSuite() {}

    void DoRun() override
    {
        TestNodeCreation();
        TestMobilityModel();
        TestAodvRoutingInstallation();
        TestInternetStackInstallation();
        TestIpAddressAssignment();
        TestUdpEchoServerSetup();
        TestUdpEchoClientSetup();
        TestDataTransmission();
    }

private:
    // Test node creation (verify that 5 nodes are created)
    void TestNodeCreation()
    {
        NodeContainer nodes;
        nodes.Create(5);

        // Verify that 5 nodes are created
        NS_TEST_ASSERT_MSG_EQ(nodes.GetN(), 5, "Failed to create the expected number of nodes.");
    }

    // Test mobility model (verify that the mobility model is installed on nodes)
    void TestMobilityModel()
    {
        NodeContainer nodes;
        nodes.Create(5);

        MobilityHelper mobility;
        mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                      "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                      "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"));
        mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                                  "Bounds", RectangleValue(Rectangle(0, 100, 0, 100)));
        mobility.Install(nodes);

        // Verify that the mobility model is installed on the nodes
        Ptr<MobilityModel> mobilityModel = nodes.Get(0)->GetObject<MobilityModel>();
        NS_TEST_ASSERT_MSG_NE(mobilityModel, nullptr, "Failed to install mobility model on node 0.");
    }

    // Test AODV routing protocol installation (verify that AODV is installed)
    void TestAodvRoutingInstallation()
    {
        NodeContainer nodes;
        nodes.Create(5);

        AodvHelper aodv;
        Ipv4ListRoutingHelper list;
        list.Add(aodv, 10);
        InternetStackHelper internet;
        internet.SetRoutingHelper(list);
        internet.Install(nodes);

        // Verify that AODV is installed on the nodes
        Ptr<Ipv4RoutingProtocol> routing = nodes.Get(0)->GetObject<Ipv4>()->GetObject<Ipv4RoutingProtocol>();
        Ptr<AodvRoutingProtocol> aodvRouting = DynamicCast<AodvRoutingProtocol>(routing);
        NS_TEST_ASSERT_MSG_NE(aodvRouting, nullptr, "Failed to install AODV routing protocol on node 0.");
    }

    // Test Internet stack installation (verify that the Internet stack is installed)
    void TestInternetStackInstallation()
    {
        NodeContainer nodes;
        nodes.Create(5);

        InternetStackHelper internet;
        internet.Install(nodes);

        // Verify that Internet stack is installed
        for (uint32_t i = 0; i < nodes.GetN(); ++i)
        {
            Ptr<Node> node = nodes.Get(i);
            Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
            NS_TEST_ASSERT_MSG_NE(ipv4, nullptr, "Internet stack was not installed on node " + std::to_string(i));
        }
    }

    // Test IP address assignment (verify that IP addresses are assigned to all nodes)
    void TestIpAddressAssignment()
    {
        NodeContainer nodes;
        nodes.Create(5);

        InternetStackHelper internet;
        internet.Install(nodes);

        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = ipv4.Assign(nodes);

        // Verify that IP addresses are assigned correctly to nodes
        for (uint32_t i = 0; i < interfaces.GetN(); ++i)
        {
            Ipv4Address ipAddr = interfaces.GetAddress(i);
            NS_TEST_ASSERT_MSG_NE(ipAddr, Ipv4Address("0.0.0.0"), "Failed to assign IP address to node " + std::to_string(i));
        }
    }

    // Test UDP echo server setup (verify that the server is set up correctly)
    void TestUdpEchoServerSetup()
    {
        NodeContainer nodes;
        nodes.Create(5);

        UdpEchoServerHelper server(9);
        ApplicationContainer serverApp = server.Install(nodes.Get(4));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        // Verify that the UDP echo server is installed on node 4
        NS_TEST_ASSERT_MSG_EQ(serverApp.GetN(), 1, "Failed to install UDP echo server on node 4.");
    }

    // Test UDP echo client setup (verify that the client is set up correctly)
    void TestUdpEchoClientSetup()
    {
        NodeContainer nodes;
        nodes.Create(5);

        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = ipv4.Assign(nodes);

        UdpEchoClientHelper client(interfaces.GetAddress(4), 9); // Node 4 as receiver
        client.SetAttribute("MaxPackets", UintegerValue(5));
        client.SetAttribute("Interval", TimeValue(Seconds(1.0)));
        client.SetAttribute("PacketSize", UintegerValue(512));
        ApplicationContainer clientApp = client.Install(nodes.Get(0));
        clientApp.Start(Seconds(1.0));
        clientApp.Stop(Seconds(10.0));

        // Verify that the UDP echo client is installed on node 0
        NS_TEST_ASSERT_MSG_EQ(clientApp.GetN(), 1, "Failed to install UDP echo client on node 0.");
    }

    // Test data transmission (verify that data is transmitted from node 0 to node 4)
    void TestDataTransmission()
    {
        NodeContainer nodes;
        nodes.Create(5);

        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = ipv4.Assign(nodes);

        UdpEchoServerHelper server(9);
        ApplicationContainer serverApp = server.Install(nodes.Get(4));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        UdpEchoClientHelper client(interfaces.GetAddress(4), 9); // Node 4 as receiver
        client.SetAttribute("MaxPackets", UintegerValue(5));
        client.SetAttribute("Interval", TimeValue(Seconds(1.0)));
        client.SetAttribute("PacketSize", UintegerValue(512));
        ApplicationContainer clientApp = client.Install(nodes.Get(0));
        clientApp.Start(Seconds(1.0));
        clientApp.Stop(Seconds(10.0));

        Simulator::Run();
        Simulator::Destroy();

        // Verify that the transmission was successful by checking received packets (not implemented here)
    }
};

int main(int argc, char *argv[])
{
    AodvTestSuite testSuite;
    testSuite.Run();
    return 0;
}
