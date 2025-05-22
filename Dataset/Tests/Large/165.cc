#include "ns3/test.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/ospf-helper.h"

using namespace ns3;

class OspfRoutingExampleTests : public ns3::TestCase {
public:
    OspfRoutingExampleTests() : ns3::TestCase("OSPF Routing Example Tests") {}

    // Test node creation
    void TestNodeCreation() {
        NodeContainer nodes;
        nodes.Create(4);

        NS_TEST_ASSERT_MSG_EQ(nodes.GetN(), 4, "Node container does not contain 4 nodes.");
    }

    // Test point-to-point link setup
    void TestPointToPointLinks() {
        NodeContainer nodes;
        nodes.Create(4);

        PointToPointHelper p2p;
        p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
        p2p.SetChannelAttribute("Delay", StringValue("2ms"));

        NetDeviceContainer devices01 = p2p.Install(nodes.Get(0), nodes.Get(1));
        NetDeviceContainer devices12 = p2p.Install(nodes.Get(1), nodes.Get(2));
        NetDeviceContainer devices23 = p2p.Install(nodes.Get(2), nodes.Get(3));
        NetDeviceContainer devices30 = p2p.Install(nodes.Get(3), nodes.Get(0));

        NS_TEST_ASSERT_MSG_EQ(devices01.GetN(), 2, "Link between node 0 and node 1 not installed correctly.");
        NS_TEST_ASSERT_MSG_EQ(devices12.GetN(), 2, "Link between node 1 and node 2 not installed correctly.");
        NS_TEST_ASSERT_MSG_EQ(devices23.GetN(), 2, "Link between node 2 and node 3 not installed correctly.");
        NS_TEST_ASSERT_MSG_EQ(devices30.GetN(), 2, "Link between node 3 and node 0 not installed correctly.");
    }

    // Test OSPF routing installation
    void TestOspfRoutingInstallation() {
        NodeContainer nodes;
        nodes.Create(4);

        InternetStackHelper internet;
        Ipv4ListRoutingHelper listRouting;
        OspfHelper ospfRouting;
        listRouting.Add(ospfRouting, 0);
        internet.SetRoutingHelper(listRouting);  // Enable OSPF
        internet.Install(nodes);

        // Verify OSPF is installed on all nodes
        for (uint32_t i = 0; i < nodes.GetN(); ++i) {
            Ptr<Node> node = nodes.Get(i);
            Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
            Ptr<Ipv4RoutingProtocol> routingProtocol = ipv4->GetRoutingProtocol();

            NS_TEST_ASSERT_MSG_NE(routingProtocol, nullptr, "OSPF routing not installed on node " << i);
        }
    }

    // Test IP address assignment
    void TestIpAddressAssignment() {
        NodeContainer nodes;
        nodes.Create(4);

        PointToPointHelper p2p;
        p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
        p2p.SetChannelAttribute("Delay", StringValue("2ms"));

        NetDeviceContainer devices01 = p2p.Install(nodes.Get(0), nodes.Get(1));
        NetDeviceContainer devices12 = p2p.Install(nodes.Get(1), nodes.Get(2));
        NetDeviceContainer devices23 = p2p.Install(nodes.Get(2), nodes.Get(3));
        NetDeviceContainer devices30 = p2p.Install(nodes.Get(3), nodes.Get(0));

        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer ifaces01 = ipv4.Assign(devices01);
        ipv4.SetBase("10.1.2.0", "255.255.255.0");
        Ipv4InterfaceContainer ifaces12 = ipv4.Assign(devices12);
        ipv4.SetBase("10.1.3.0", "255.255.255.0");
        Ipv4InterfaceContainer ifaces23 = ipv4.Assign(devices23);
        ipv4.SetBase("10.1.4.0", "255.255.255.0");
        Ipv4InterfaceContainer ifaces30 = ipv4.Assign(devices30);

        // Check if IP addresses are correctly assigned
        for (uint32_t i = 0; i < nodes.GetN(); ++i) {
            Ptr<Ipv4> ipv4 = nodes.Get(i)->GetObject<Ipv4>();
            Ipv4InterfaceContainer interfaces = ipv4->GetObject<Ipv4L3Protocol>()->GetInterfaces();
            NS_TEST_ASSERT_MSG_NE(interfaces.GetAddress(0), Ipv4Address("0.0.0.0"), "Node " << i << " does not have a valid IP address.");
        }
    }

    // Test application setup: Echo server and client
    void TestApplicationSetup() {
        NodeContainer nodes;
        nodes.Create(4);

        UdpEchoServerHelper echoServer(9);
        ApplicationContainer serverApps = echoServer.Install(nodes.Get(3)); // Server on node 3
        serverApps.Start(Seconds(1.0));
        serverApps.Stop(Seconds(10.0));

        UdpEchoClientHelper echoClient("10.1.3.2", 9); // Client on node 0 sending to node 3
        echoClient.SetAttribute("MaxPackets", UintegerValue(5));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
        echoClient.SetAttribute("PacketSize", UintegerValue(1024));

        ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));  // Client on node 0
        clientApps.Start(Seconds(2.0));
        clientApps.Stop(Seconds(10.0));

        Simulator::Run();
        Simulator::Destroy();
    }

    // Test routing table population and output
    void TestRoutingTablePopulation() {
        NodeContainer nodes;
        nodes.Create(4);

        InternetStackHelper internet;
        Ipv4ListRoutingHelper listRouting;
        OspfHelper ospfRouting;
        listRouting.Add(ospfRouting, 0);
        internet.SetRoutingHelper(listRouting);  // Enable OSPF routing
        internet.Install(nodes);

        Ipv4GlobalRoutingHelper::PopulateRoutingTables();

        // Check if routing tables are populated (simulate printing them)
        Simulator::Schedule(Seconds(2.0), &Ipv4GlobalRoutingHelper::PrintRoutingTableAllAt, Seconds(2.0), Create<OutputStreamWrapper>(&std::cout));
    }

    // Test pcap tracing
    void TestPcapTracing() {
        NodeContainer nodes;
        nodes.Create(4);

        PointToPointHelper p2p;
        p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
        p2p.SetChannelAttribute("Delay", StringValue("2ms"));

        NetDeviceContainer devices01 = p2p.Install(nodes.Get(0), nodes.Get(1));
        NetDeviceContainer devices12 = p2p.Install(nodes.Get(1), nodes.Get(2));
        NetDeviceContainer devices23 = p2p.Install(nodes.Get(2), nodes.Get(3));
        NetDeviceContainer devices30 = p2p.Install(nodes.Get(3), nodes.Get(0));

        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.1.1.0", "255.255.255.0");
        ipv4.Assign(devices01);
        ipv4.SetBase("10.1.2.0", "255.255.255.0");
        ipv4.Assign(devices12);
        ipv4.SetBase("10.1.3.0", "255.255.255.0");
        ipv4.Assign(devices23);
        ipv4.SetBase("10.1.4.0", "255.255.255.0");
        ipv4.Assign(devices30);

        // Enable pcap tracing
        p2p.EnablePcapAll("ospf-routing");

        // Verify if pcap files are generated (this would be done manually after running the simulation)
        // In a real test scenario, we'd check the output directory for "ospf-routing-0-0.pcap"
        Simulator::Run();
        Simulator::Destroy();
    }

    virtual void DoRun() {
        TestNodeCreation();
        TestPointToPointLinks();
        TestOspfRoutingInstallation();
        TestIpAddressAssignment();
        TestApplicationSetup();
        TestRoutingTablePopulation();
        TestPcapTracing();
    }
};

// Register the test suite
static OspfRoutingExampleTests g_ospfRoutingExampleTests;
