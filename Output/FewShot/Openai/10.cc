#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/network-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("CsmaUdpEchoIpv4v6Example");

static void
PacketReceiveCallback(Ptr<const Packet> packet, const Address &addr)
{
    NS_LOG_INFO("Packet received at " << Simulator::Now ().GetSeconds () << "s, " <<
        "Size: " << packet->GetSize () << " bytes");
}

int main(int argc, char *argv[])
{
    bool useIpv6 = false;
    CommandLine cmd;
    cmd.AddValue("useIpv6", "Enable IPv6 addressing", useIpv6);
    cmd.Parse(argc, argv);

    LogComponentEnable("CsmaUdpEchoIpv4v6Example", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(4);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("5Mbps"));
    csma.SetChannelAttribute("Delay", StringValue("2ms"));
    csma.SetDeviceAttribute("Mtu", UintegerValue(1400));

    NetDeviceContainer devices = csma.Install(nodes);

    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv4InterfaceContainer ipv4Ifs;
    Ipv6InterfaceContainer ipv6Ifs;

    if (!useIpv6)
    {
        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.1.1.0", "255.255.255.0");
        ipv4Ifs = ipv4.Assign(devices);
    }
    else
    {
        Ipv6AddressHelper ipv6;
        ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
        ipv6Ifs = ipv6.Assign(devices);
        for (uint32_t i = 0; i < nodes.GetN(); ++i)
        {
            ipv6Ifs.SetForwarding(i, true);
            ipv6Ifs.SetDefaultRouteInAllNodes(i);
        }
    }

    // Set up UDP Echo Server on n1
    uint16_t echoPort = 9;
    UdpEchoServerHelper echoServer(echoPort);
    ApplicationContainer serverApp = echoServer.Install(nodes.Get(1));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(11.0));

    // Set up UDP Echo Client on n0
    Address serverAddress;
    if (!useIpv6)
    {
        serverAddress = InetSocketAddress(ipv4Ifs.GetAddress(1), echoPort);
    }
    else
    {
        serverAddress = Inet6SocketAddress(ipv6Ifs.GetAddress(1, 1), echoPort);
    }

    UdpEchoClientHelper echoClient(serverAddress);
    echoClient.SetAttribute("MaxPackets", UintegerValue(8));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = echoClient.Install(nodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Enable queue tracing
    std::string queueTrFile = "udp-echo.tr";
    AsciiTraceHelper ascii;
    Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream(queueTrFile);

    csma.EnableAsciiAll(stream);
    csma.EnablePcapAll("udp-echo");

    // Additional logging: connect packet reception callback on n1
    Ptr<Node> node1 = nodes.Get(1);
    Ptr<NetDevice> dev1 = devices.Get(1);

    // Trace Ipv4 packet receptions
    if (!useIpv6)
    {
        Ptr<Ipv4L3Protocol> ipv4L3 = node1->GetObject<Ipv4L3Protocol>();
        if (ipv4L3)
        {
            for (uint32_t i = 0; i < ipv4L3->GetNInterfaces(); ++i)
            {
                ipv4L3->TraceConnectWithoutContext("Rx", 
                    MakeCallback(&PacketReceiveCallback)
                );
            }
        }
    }
    else
    {
        Ptr<Ipv6L3Protocol> ipv6L3 = node1->GetObject<Ipv6L3Protocol>();
        if (ipv6L3)
        {
            for (uint32_t i = 0; i < ipv6L3->GetNInterfaces(); ++i)
            {
                ipv6L3->TraceConnectWithoutContext("Rx",
                    MakeCallback(&PacketReceiveCallback)
                );
            }
        }
    }

    Simulator::Stop(Seconds(12.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}