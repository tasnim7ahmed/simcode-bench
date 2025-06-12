#include "ns3/test.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

class TcpStarServerTest : public TestCase
{
public:
    TcpStarServerTest() : TestCase("Test TcpStarServer") {}

    void DoRun() override
    {
        TestNodeCreation();
        TestApplicationInstallation();
        TestPacketSent();
        TestIPAssignment();
        TestTracing();
    }

private:
    void TestNodeCreation()
    {
        uint32_t N = 9; // number of nodes in the star
        NodeContainer serverNode;
        NodeContainer clientNodes;
        serverNode.Create(1);
        clientNodes.Create(N - 1);
        NodeContainer allNodes = NodeContainer(serverNode, clientNodes);

        // Verify that we have created the correct number of nodes
        NS_ASSERT_EQUAL(allNodes.GetN(), N);
    }

    void TestApplicationInstallation()
    {
        uint32_t N = 9;
        NodeContainer serverNode;
        NodeContainer clientNodes;
        serverNode.Create(1);
        clientNodes.Create(N - 1);
        NodeContainer allNodes = NodeContainer(serverNode, clientNodes);

        InternetStackHelper internet;
        internet.Install(allNodes);

        // Create PacketSink (server) and OnOff (client) applications
        uint16_t port = 50000;
        Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
        PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
        ApplicationContainer sinkApp = sinkHelper.Install(serverNode);
        sinkApp.Start(Seconds(1.0));
        sinkApp.Stop(Seconds(10.0));

        OnOffHelper clientHelper("ns3::TcpSocketFactory", Address());
        clientHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
        clientHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));

        ApplicationContainer clientApps;
        for (uint32_t i = 0; i < clientNodes.GetN(); ++i)
        {
            AddressValue remoteAddress(InetSocketAddress(Ipv4Address("10.1.1.1"), port));
            clientHelper.SetAttribute("Remote", remoteAddress);
            clientApps.Add(clientHelper.Install(clientNodes.Get(i)));
        }

        clientApps.Start(Seconds(1.0));
        clientApps.Stop(Seconds(10.0));

        // Verify that applications are installed correctly
        NS_ASSERT_EQUAL(sinkApp.GetN(), 1);
        NS_ASSERT_EQUAL(clientApps.GetN(), N - 1);
    }

    void TestPacketSent()
    {
        uint32_t N = 9;
        NodeContainer serverNode;
        NodeContainer clientNodes;
        serverNode.Create(1);
        clientNodes.Create(N - 1);
        NodeContainer allNodes = NodeContainer(serverNode, clientNodes);

        PointToPointHelper p2p;
        p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
        p2p.SetChannelAttribute("Delay", StringValue("2ms"));

        std::vector<NetDeviceContainer> deviceAdjacencyList(N - 1);
        for (uint32_t i = 0; i < deviceAdjacencyList.size(); ++i)
        {
            NodeContainer nodes = NodeContainer(serverNode, clientNodes.Get(i));
            deviceAdjacencyList[i] = p2p.Install(nodes);
        }

        InternetStackHelper internet;
        internet.Install(allNodes);

        uint16_t port = 50000;
        Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
        PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
        ApplicationContainer sinkApp = sinkHelper.Install(serverNode);
        sinkApp.Start(Seconds(1.0));
        sinkApp.Stop(Seconds(10.0));

        OnOffHelper clientHelper("ns3::TcpSocketFactory", Address());
        clientHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
        clientHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));

        ApplicationContainer clientApps;
        for (uint32_t i = 0; i < clientNodes.GetN(); ++i)
        {
            AddressValue remoteAddress(InetSocketAddress(Ipv4Address("10.1.1.1"), port));
            clientHelper.SetAttribute("Remote", remoteAddress);
            clientApps.Add(clientHelper.Install(clientNodes.Get(i)));
        }

        clientApps.Start(Seconds(1.0));
        clientApps.Stop(Seconds(10.0));

        Simulator::Run();

        Ptr<PacketSink> sink1 = DynamicCast<PacketSink>(sinkApp.Get(0));
        uint32_t receivedBytes = sink1->GetTotalRx();

        // Ensure that some data is received
        NS_ASSERT_GREATER(receivedBytes, 0);
    }

    void TestIPAssignment()
    {
        uint32_t N = 9;
        NodeContainer serverNode;
        NodeContainer clientNodes;
        serverNode.Create(1);
        clientNodes.Create(N - 1);
        NodeContainer allNodes = NodeContainer(serverNode, clientNodes);

        PointToPointHelper p2p;
        p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
        p2p.SetChannelAttribute("Delay", StringValue("2ms"));

        std::vector<NetDeviceContainer> deviceAdjacencyList(N - 1);
        Ipv4AddressHelper ipv4;
        std::vector<Ipv4InterfaceContainer> interfaceAdjacencyList(N - 1);

        for (uint32_t i = 0; i < deviceAdjacencyList.size(); ++i)
        {
            NodeContainer nodes = NodeContainer(serverNode, clientNodes.Get(i));
            deviceAdjacencyList[i] = p2p.Install(nodes);

            std::ostringstream subnet;
            subnet << "10.1." << i + 1 << ".0";
            ipv4.SetBase(subnet.str().c_str(), "255.255.255.0");
            interfaceAdjacencyList[i] = ipv4.Assign(deviceAdjacencyList[i]);
        }

        // Verify that IP addresses are correctly assigned to the first client node
        Ipv4Address assignedIP = interfaceAdjacencyList[0].GetAddress(0);
        NS_ASSERT_TRUE(assignedIP != Ipv4Address("0.0.0.0"));
    }

    void TestTracing()
    {
        uint32_t N = 9;
        NodeContainer serverNode;
        NodeContainer clientNodes;
        serverNode.Create(1);
        clientNodes.Create(N - 1);
        NodeContainer allNodes = NodeContainer(serverNode, clientNodes);

        PointToPointHelper p2p;
        p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
        p2p.SetChannelAttribute("Delay", StringValue("2ms"));

        std::vector<NetDeviceContainer> deviceAdjacencyList(N - 1);
        for (uint32_t i = 0; i < deviceAdjacencyList.size(); ++i)
        {
            NodeContainer nodes = NodeContainer(serverNode, clientNodes.Get(i));
            deviceAdjacencyList[i] = p2p.Install(nodes);
        }

        InternetStackHelper internet;
        internet.Install(allNodes);

        // Enable tracing
        AsciiTraceHelper ascii;
        p2p.EnableAsciiAll(ascii.CreateFileStream("tcp-star-server.tr"));
        p2p.EnablePcapAll("tcp-star-server");

        Simulator::Run();

        // Check if trace file is created
        std::ifstream traceFile("tcp-star-server.tr");
        NS_ASSERT_TRUE(traceFile.is_open());
    }
};

// Register the test
TestSuite* CreateTcpStarServerTestSuite()
{
    TestSuite* suite = new TestSuite("TcpStarServer", SYSTEM);
    suite->AddTestCase(new TcpStarServerTest());
    return suite;
}

