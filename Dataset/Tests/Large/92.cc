#include "ns3/test.h" // Use ns-3 testing framework
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/point-to-point-module.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/socket.h"
#include "ns3/ptr.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("SocketBoundTcpRoutingTest");

class MultiInterfaceRoutingTest : public TestCase
{
public:
    MultiInterfaceRoutingTest() : TestCase("Multi-Interface Static Routing Test") {}

    void DoRun() override
    {
        // Set up nodes and devices
        Ptr<Node> nSrc = CreateObject<Node>();
        Ptr<Node> nDst = CreateObject<Node>();
        Ptr<Node> nRtr1 = CreateObject<Node>();
        Ptr<Node> nRtr2 = CreateObject<Node>();
        Ptr<Node> nDstRtr = CreateObject<Node>();

        NodeContainer c = NodeContainer(nSrc, nDst, nRtr1, nRtr2, nDstRtr);

        InternetStackHelper internet;
        internet.Install(c);

        // Create Point-to-Point links
        PointToPointHelper p2p;
        p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
        p2p.SetChannelAttribute("Delay", StringValue("2ms"));
        NetDeviceContainer dSrcdRtr1 = p2p.Install(NodeContainer(nSrc, nRtr1));
        NetDeviceContainer dSrcdRtr2 = p2p.Install(NodeContainer(nSrc, nRtr2));
        NetDeviceContainer dRtr1dDstRtr = p2p.Install(NodeContainer(nRtr1, nDstRtr));
        NetDeviceContainer dRtr2dDstRtr = p2p.Install(NodeContainer(nRtr2, nDstRtr));
        NetDeviceContainer dDstRtrdDst = p2p.Install(NodeContainer(nDstRtr, nDst));

        // Assign IPs
        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.1.1.0", "255.255.255.0");
        ipv4.Assign(dSrcdRtr1);
        ipv4.SetBase("10.1.2.0", "255.255.255.0");
        ipv4.Assign(dSrcdRtr2);
        ipv4.SetBase("10.10.1.0", "255.255.255.0");
        ipv4.Assign(dRtr1dDstRtr);
        ipv4.SetBase("10.10.2.0", "255.255.255.0");
        ipv4.Assign(dRtr2dDstRtr);
        ipv4.SetBase("10.20.1.0", "255.255.255.0");
        ipv4.Assign(dDstRtrdDst);

        // Verify static routing on the source node
        Ptr<Ipv4> ipv4Src = nSrc->GetObject<Ipv4>();
        Ipv4StaticRoutingHelper ipv4RoutingHelper;
        Ptr<Ipv4StaticRouting> staticRoutingSrc = ipv4RoutingHelper.GetStaticRouting(ipv4Src);

        // Add static route
        staticRoutingSrc->AddHostRouteTo(Ipv4Address("10.20.1.2"), Ipv4Address("10.1.1.2"), 1, 5);
        
        // Verify routing table entry for static route to destination
        Ipv4RoutingTableEntry entry = staticRoutingSrc->GetRoute(0);
        NS_TEST_ASSERT_MSG_EQ(entry.GetDest(), Ipv4Address("10.20.1.2"), "Static route not correctly added.");
    }
};

class SocketBindingTest : public TestCase
{
public:
    SocketBindingTest() : TestCase("Socket Binding Test") {}

    void DoRun() override
    {
        // Setup nodes and network devices
        Ptr<Node> nSrc = CreateObject<Node>();
        Ptr<Node> nDst = CreateObject<Node>();
        Ptr<Node> nRtr1 = CreateObject<Node>();

        NodeContainer c = NodeContainer(nSrc, nDst, nRtr1);
        InternetStackHelper internet;
        internet.Install(c);

        PointToPointHelper p2p;
        p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
        p2p.SetChannelAttribute("Delay", StringValue("2ms"));
        NetDeviceContainer dSrcdRtr1 = p2p.Install(NodeContainer(nSrc, nRtr1));
        NetDeviceContainer dRtr1dDst = p2p.Install(NodeContainer(nRtr1, nDst));

        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.1.1.0", "255.255.255.0");
        ipv4.Assign(dSrcdRtr1);
        ipv4.SetBase("10.10.1.0", "255.255.255.0");
        ipv4.Assign(dRtr1dDst);

        Ptr<Socket> srcSocket = Socket::CreateSocket(nSrc, TypeId::LookupByName("ns3::TcpSocketFactory"));
        Ptr<NetDevice> SrcToRtr1 = dSrcdRtr1.Get(0);
        
        // Bind the socket to the device
        BindSock(srcSocket, SrcToRtr1);

        // Verify if the socket is bound correctly
        NS_TEST_ASSERT_MSG_EQ(srcSocket->GetBoundNetDevice(), SrcToRtr1, "Socket was not bound to the correct net device.");
    }
};

class PacketTransmissionTest : public TestCase
{
public:
    PacketTransmissionTest() : TestCase("Packet Transmission Test") {}

    void DoRun() override
    {
        // Create nodes, devices and configure network
        Ptr<Node> nSrc = CreateObject<Node>();
        Ptr<Node> nDst = CreateObject<Node>();
        Ptr<Node> nRtr1 = CreateObject<Node>();
        Ptr<Node> nRtr2 = CreateObject<Node>();
        Ptr<Node> nDstRtr = CreateObject<Node>();

        NodeContainer c = NodeContainer(nSrc, nDst, nRtr1, nRtr2, nDstRtr);
        InternetStackHelper internet;
        internet.Install(c);

        PointToPointHelper p2p;
        p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
        p2p.SetChannelAttribute("Delay", StringValue("2ms"));
        NetDeviceContainer dSrcdRtr1 = p2p.Install(NodeContainer(nSrc, nRtr1));
        NetDeviceContainer dSrcdRtr2 = p2p.Install(NodeContainer(nSrc, nRtr2));
        NetDeviceContainer dRtr1dDstRtr = p2p.Install(NodeContainer(nRtr1, nDstRtr));
        NetDeviceContainer dRtr2dDstRtr = p2p.Install(NodeContainer(nRtr2, nDstRtr));
        NetDeviceContainer dDstRtrdDst = p2p.Install(NodeContainer(nDstRtr, nDst));

        // Assign IPs
        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.1.1.0", "255.255.255.0");
        ipv4.Assign(dSrcdRtr1);
        ipv4.SetBase("10.1.2.0", "255.255.255.0");
        ipv4.Assign(dSrcdRtr2);
        ipv4.SetBase("10.10.1.0", "255.255.255.0");
        ipv4.Assign(dRtr1dDstRtr);
        ipv4.SetBase("10.10.2.0", "255.255.255.0");
        ipv4.Assign(dRtr2dDstRtr);
        ipv4.SetBase("10.20.1.0", "255.255.255.0");
        ipv4.Assign(dDstRtrdDst);

        // Set up packet sink
        uint16_t dstport = 12345;
        PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), dstport));
        ApplicationContainer apps = sink.Install(nDst);
        apps.Start(Seconds(0.0));
        apps.Stop(Seconds(10.0));

        // Simulate a flow and check if packets are received
        Simulator::Run();

        // Verify if the expected number of packets were received
        Ptr<PacketSink> packetSink = DynamicCast<PacketSink>(apps.Get(0));
        NS_TEST_ASSERT_MSG_EQ(packetSink->GetTotalRx(), totalTxBytes, "Packet transmission failed.");
    }
};

// Register test cases
static MultiInterfaceRoutingTest multiInterfaceRoutingTest;
static SocketBindingTest socketBindingTest;
static PacketTransmissionTest packetTransmissionTest;
