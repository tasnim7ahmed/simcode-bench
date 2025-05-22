#include "ns3/test.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/udp-client-server-helper.h"

using namespace ns3;

class TcpUdpComparisonTests : public ns3::TestCase {
public:
    TcpUdpComparisonTests() : ns3::TestCase("TcpUdpComparison Test Cases") {}

    // Test UDP server installation
    void TestUdpServerInstallation() {
        NodeContainer nodes;
        nodes.Create(2);
        UdpServerHelper udpServer(9);
        ApplicationContainer serverApp = udpServer.Install(nodes.Get(1));

        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        // Check if the server is installed
        NS_TEST_ASSERT_MSG_EQ(serverApp.Get(0)->GetState(), Application::STARTED, "UDP Server did not start as expected");
    }

    // Test UDP client installation
    void TestUdpClientInstallation() {
        NodeContainer nodes;
        nodes.Create(2);
        UdpClientHelper udpClient("10.1.1.2", 9);
        udpClient.SetAttribute("MaxPackets", UintegerValue(1000));
        udpClient.SetAttribute("Interval", TimeValue(Seconds(0.05)));
        udpClient.SetAttribute("PacketSize", UintegerValue(1024));

        ApplicationContainer clientApp = udpClient.Install(nodes.Get(0));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));

        // Check if the client application is installed
        NS_TEST_ASSERT_MSG_EQ(clientApp.Get(0)->GetState(), Application::STARTED, "UDP Client did not start as expected");
    }

    // Test TCP server (Packet Sink) installation
    void TestTcpServerInstallation() {
        NodeContainer nodes;
        nodes.Create(2);
        PacketSinkHelper tcpSink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), 8080));
        ApplicationContainer sinkApp = tcpSink.Install(nodes.Get(1));

        sinkApp.Start(Seconds(1.0));
        sinkApp.Stop(Seconds(10.0));

        // Verify that the TCP sink application is installed and started
        NS_TEST_ASSERT_MSG_EQ(sinkApp.Get(0)->GetState(), Application::STARTED, "TCP Server (Packet Sink) did not start as expected");
    }

    // Test TCP client installation
    void TestTcpClientInstallation() {
        NodeContainer nodes;
        nodes.Create(2);
        OnOffHelper tcpClient("ns3::TcpSocketFactory", InetSocketAddress("10.1.1.2", 8080));
        tcpClient.SetAttribute("DataRate", StringValue("10Mbps"));
        tcpClient.SetAttribute("PacketSize", UintegerValue(1024));
        tcpClient.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
        tcpClient.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));

        ApplicationContainer clientAppTcp = tcpClient.Install(nodes.Get(0));
        clientAppTcp.Start(Seconds(3.0));
        clientAppTcp.Stop(Seconds(10.0));

        // Check if the TCP client application is installed and started
        NS_TEST_ASSERT_MSG_EQ(clientAppTcp.Get(0)->GetState(), Application::STARTED, "TCP Client did not start as expected");
    }

    // Test Flow Monitor to ensure packet stats are generated
    void TestFlowMonitor() {
        NodeContainer nodes;
        nodes.Create(2);

        // Setup the PointToPoint link
        PointToPointHelper pointToPoint;
        pointToPoint.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
        pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));
        NetDeviceContainer devices = pointToPoint.Install(nodes);

        // Install the Internet stack
        InternetStackHelper stack;
        stack.Install(nodes);

        // Assign IP addresses
        Ipv4AddressHelper address;
        address.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = address.Assign(devices);

        // Create applications
        UdpServerHelper udpServer(9);
        ApplicationContainer serverApp = udpServer.Install(nodes.Get(1));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        UdpClientHelper udpClient(interfaces.GetAddress(1), 9);
        udpClient.SetAttribute("MaxPackets", UintegerValue(1000));
        udpClient.SetAttribute("Interval", TimeValue(Seconds(0.05)));
        udpClient.SetAttribute("PacketSize", UintegerValue(1024));

        ApplicationContainer clientApp = udpClient.Install(nodes.Get(0));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));

        // Set up FlowMonitor
        FlowMonitorHelper flowMonitorHelper;
        Ptr<FlowMonitor> flowMonitor = flowMonitorHelper.InstallAll();

        // Run simulation
        Simulator::Stop(Seconds(10));
        Simulator::Run();

        // Check the flow monitor statistics
        flowMonitor->CheckForLostPackets();
        Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowMonitorHelper.GetClassifier());
        FlowMonitor::FlowStatsContainer stats = flowMonitor->GetFlowStats();

        for (auto it = stats.begin(); it != stats.end(); ++it) {
            Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(it->first);
            NS_TEST_ASSERT_MSG_EQ(it->second.rxPackets, 1000, "Unexpected received UDP packets count");
        }

        Simulator::Destroy();
    }

    // Test NetAnim configuration
    void TestNetAnimConfiguration() {
        NodeContainer nodes;
        nodes.Create(2);
        AnimationInterface anim("tcp_vs_udp.xml");

        anim.SetConstantPosition(nodes.Get(0), 10.0, 20.0);
        anim.SetConstantPosition(nodes.Get(1), 30.0, 20.0);

        // Check if the nodes positions are set correctly
        Vector pos0 = anim.GetNodePosition(nodes.Get(0));
        Vector pos1 = anim.GetNodePosition(nodes.Get(1));

        NS_TEST_ASSERT_MSG_EQ(pos0.x, 10.0, "Node 0 position not set correctly");
        NS_TEST_ASSERT_MSG_EQ(pos0.y, 20.0, "Node 0 position not set correctly");
        NS_TEST_ASSERT_MSG_EQ(pos1.x, 30.0, "Node 1 position not set correctly");
        NS_TEST_ASSERT_MSG_EQ(pos1.y, 20.0, "Node 1 position not set correctly");
    }

    virtual void DoRun() {
        TestUdpServerInstallation();
        TestUdpClientInstallation();
        TestTcpServerInstallation();
        TestTcpClientInstallation();
        TestFlowMonitor();
        TestNetAnimConfiguration();
    }
};

// Register the test suite
static TcpUdpComparisonTests g_tcpUdpComparisonTests;
