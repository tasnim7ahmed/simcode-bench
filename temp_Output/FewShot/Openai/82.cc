#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-address-helper.h"
#include <fstream>

using namespace ns3;

void
PacketRxTrace(Ptr<const Packet> packet)
{
    static std::ofstream rxStream("udp-echo.tr", std::ios::app);
    rxStream << Simulator::Now().GetSeconds() << " RX " << packet->GetSize() << " bytes" << std::endl;
}

void
QueueLenTrace(uint32_t oldValue, uint32_t newValue)
{
    static std::ofstream qStream("udp-echo.tr", std::ios::app);
    qStream << Simulator::Now().GetSeconds() << " QueueLength " << newValue << std::endl;
}

int
main(int argc, char *argv[])
{
    bool useIpv6 = false;
    CommandLine cmd;
    cmd.AddValue("useIpv6", "Use IPv6 instead of IPv4", useIpv6);
    cmd.Parse(argc, argv);

    // Reset trace file
    std::ofstream clearFile("udp-echo.tr", std::ofstream::out | std::ofstream::trunc);
    clearFile.close();

    NodeContainer nodes;
    nodes.Create(4);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", StringValue("2ms"));
    csma.SetQueue("ns3::DropTailQueue", "MaxPackets", UintegerValue(100));

    NetDeviceContainer devices = csma.Install(nodes);

    // Trace queue length
    Ptr<Queue<Packet>> queue = DynamicCast<Queue<Packet>>(devices.Get(0)->GetObject<PointToPointNetDevice>()->GetQueue());
    // Actually, for CsmaNetDevice, the queue is attached to the channel
    Ptr<CsmaNetDevice> csmaDev = DynamicCast<CsmaNetDevice>(devices.Get(0));
    Ptr<Queue<Packet>> csmaQueue = csmaDev->GetQueue();
    if (csmaQueue)
    {
        csmaQueue->TraceConnect("PacketsInQueue", std::string(), MakeCallback(&QueueLenTrace));
    }

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4InterfaceContainer interfaces4;
    Ipv6InterfaceContainer interfaces6;

    if (!useIpv6)
    {
        Ipv4AddressHelper address;
        address.SetBase("10.1.1.0", "255.255.255.0");
        interfaces4 = address.Assign(devices);
    }
    else
    {
        Ipv6AddressHelper address6;
        address6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
        interfaces6 = address6.Assign(devices);
        for (uint32_t i = 0; i < interfaces6.GetN(); ++i)
        {
            interfaces6.SetForwarding(i, true);
            interfaces6.SetDefaultRouteInAllNodes(i);
        }
    }

    uint16_t port = 9;

    // UDP Echo Server on n1
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApp = echoServer.Install(nodes.Get(1));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // UDP Echo Client on n0 talking to n1
    Address serverAddr;
    if (!useIpv6)
    {
        serverAddr = Address(interfaces4.GetAddress(1));
    }
    else
    {
        serverAddr = Address(interfaces6.GetAddress(1, 1)); // second param is interface index, 1 is global
    }
    UdpEchoClientHelper echoClient(serverAddr, port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = echoClient.Install(nodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Trace packet reception on server
    Ptr<Node> serverNode = nodes.Get(1);
    Ptr<Application> server = serverApp.Get(0);
    Ptr<UdpEchoServer> udpServer = DynamicCast<UdpEchoServer>(server);
    if (udpServer)
    {
        udpServer->TraceConnectWithoutContext("Rx", MakeCallback(&PacketRxTrace));
    }

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}