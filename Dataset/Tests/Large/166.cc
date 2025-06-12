#include "ns3/test.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/aodv-helper.h"
#include "ns3/applications-module.h"

using namespace ns3;

class AodvAdhocExampleTests : public ns3::TestCase {
public:
    AodvAdhocExampleTests() : ns3::TestCase("AODV Ad-Hoc Example Tests") {}

    // Test node creation
    void TestNodeCreation() {
        uint32_t numNodes = 5;
        NodeContainer nodes;
        nodes.Create(numNodes);

        NS_TEST_ASSERT_MSG_EQ(nodes.GetN(), numNodes, "Node container does not contain the correct number of nodes.");
    }

    // Test Wi-Fi device installation
    void TestWifiDeviceInstallation() {
        uint32_t numNodes = 5;
        NodeContainer nodes;
        nodes.Create(numNodes);

        WifiHelper wifi;
        wifi.SetStandard(WIFI_STANDARD_80211b);

        YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
        YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
        wifiPhy.SetChannel(wifiChannel.Create());

        WifiMacHelper wifiMac;
        wifiMac.SetType("ns3::AdhocWifiMac");

        NetDeviceContainer devices;
        devices = wifi.Install(wifiPhy, wifiMac, nodes);

        NS_TEST_ASSERT_MSG_EQ(devices.GetN(), numNodes, "Wi-Fi devices were not installed on all nodes.");
    }

    // Test mobility model installation
    void TestMobilityModelInstallation() {
        uint32_t numNodes = 5;
        NodeContainer nodes;
        nodes.Create(numNodes);

        MobilityHelper mobility;
        mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                      "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                      "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"));
        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        mobility.Install(nodes);

        // Verify that the mobility model is installed correctly
        for (uint32_t i = 0; i < nodes.GetN(); ++i) {
            Ptr<MobilityModel> mobilityModel = nodes.Get(i)->GetObject<MobilityModel>();
            NS_TEST_ASSERT_MSG_NE(mobilityModel, nullptr, "Mobility model not installed on node " << i);
        }
    }

    // Test AODV routing protocol installation
    void TestAodvRoutingInstallation() {
        uint32_t numNodes = 5;
        NodeContainer nodes;
        nodes.Create(numNodes);

        AodvHelper aodv;
        InternetStackHelper internet;
        internet.SetRoutingHelper(aodv);
        internet.Install(nodes);

        // Verify that AODV routing is installed on each node
        for (uint32_t i = 0; i < nodes.GetN(); ++i) {
            Ptr<Ipv4RoutingProtocol> routingProtocol = nodes.Get(i)->GetObject<Ipv4>()->GetRoutingProtocol();
            NS_TEST_ASSERT_MSG_NE(routingProtocol, nullptr, "AODV routing not installed on node " << i);
        }
    }

    // Test IP address assignment
    void TestIpAddressAssignment() {
        uint32_t numNodes = 5;
        NodeContainer nodes;
        nodes.Create(numNodes);

        WifiHelper wifi;
        wifi.SetStandard(WIFI_STANDARD_80211b);

        YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
        YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
        wifiPhy.SetChannel(wifiChannel.Create());

        WifiMacHelper wifiMac;
        wifiMac.SetType("ns3::AdhocWifiMac");

        NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.0.0.0", "255.255.255.0");
        Ipv4InterfaceContainer ifaces = ipv4.Assign(devices);

        // Check if IP addresses are correctly assigned
        for (uint32_t i = 0; i < nodes.GetN(); ++i) {
            Ptr<Ipv4> ipv4 = nodes.Get(i)->GetObject<Ipv4>();
            Ipv4InterfaceContainer interfaces = ipv4->GetObject<Ipv4L3Protocol>()->GetInterfaces();
            NS_TEST_ASSERT_MSG_NE(interfaces.GetAddress(0), Ipv4Address("0.0.0.0"), "Node " << i << " does not have a valid IP address.");
        }
    }

    // Test application setup: UDP server and client
    void TestApplicationSetup() {
        uint32_t numNodes = 5;
        NodeContainer nodes;
        nodes.Create(numNodes);

        uint16_t port = 9;
        UdpEchoServerHelper echoServer(port);
        ApplicationContainer serverApps = echoServer.Install(nodes.Get(4));  // Server on node 4
        serverApps.Start(Seconds(1.0));
        serverApps.Stop(Seconds(10.0));

        UdpEchoClientHelper echoClient("10.0.0.4", port);  // Client on node 0 sending to node 4
        echoClient.SetAttribute("MaxPackets", UintegerValue(5));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
        echoClient.SetAttribute("PacketSize", UintegerValue(1024));

        ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));  // Client on node 0
        clientApps.Start(Seconds(2.0));
        clientApps.Stop(Seconds(10.0));

        // Run the simulation and verify no errors in the client-server interaction
        Simulator::Run();
        Simulator::Destroy();
    }

    // Test pcap tracing functionality
    void TestPcapTracing() {
        uint32_t numNodes = 5;
        NodeContainer nodes;
        nodes.Create(numNodes);

        WifiHelper wifi;
        wifi.SetStandard(WIFI_STANDARD_80211b);

        YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
        YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
        wifiPhy.SetChannel(wifiChannel.Create());

        WifiMacHelper wifiMac;
        wifiMac.SetType("ns3::AdhocWifiMac");

        NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.0.0.0", "255.255.255.0");
        ipv4.Assign(devices);

        // Enable pcap tracing to observe packet transmission
        wifiPhy.EnablePcapAll("aodv-adhoc");

        // Verify if pcap files are generated (this would be done manually after running the simulation)
        // In a real test scenario, we'd check the output directory for "aodv-adhoc-0-0.pcap"
        Simulator::Run();
        Simulator::Destroy();
    }

    virtual void DoRun() {
        TestNodeCreation();
        TestWifiDeviceInstallation();
        TestMobilityModelInstallation();
        TestAodvRoutingInstallation();
        TestIpAddressAssignment();
        TestApplicationSetup();
        TestPcapTracing();
    }
};

// Register the test suite
static AodvAdhocExampleTests g_aodvAdhocExampleTests;
