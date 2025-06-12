#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-address-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    bool useIpv6 = false;
    CommandLine cmd(__FILE__);
    cmd.AddValue("useIpv6", "Use IPv6 if true, IPv4 otherwise", useIpv6);
    cmd.Parse(argc, argv);

    // Enable logging
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create 4 nodes
    NodeContainer nodes;
    nodes.Create(4);

    // LAN setup using PointToPoint for all connections (full mesh)
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Install DropTail queue
    p2p.SetQueue("ns3::DropTailQueue<Packet>");

    NetDeviceContainer devices[4][4];  // To store all point-to-point links

    for (uint32_t i = 0; i < 4; ++i) {
        for (uint32_t j = i + 1; j < 4; ++j) {
            devices[i][j] = p2p.Install(NodeContainer(nodes.Get(i), nodes.Get(j)));
        }
    }

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    if (!useIpv6) {
        Ipv4AddressHelper address;
        address.SetBase("10.1.1.0", "255.255.255.0");
        for (uint32_t i = 0; i < 4; ++i) {
            for (uint32_t j = i + 1; j < 4; ++j) {
                address.Assign(devices[i][j]);
                address.NewNetwork();
            }
        }
    } else {
        Ipv6AddressHelper address;
        address.SetBase(Ipv6Address("2001:db8::"), Ipv6Prefix(64));
        for (uint32_t i = 0; i < 4; ++i) {
            for (uint32_t j = i + 1; j < 4; ++j) {
                address.Assign(devices[i][j]);
                address.NewNetwork();
            }
        }
    }

    // Set up UDP Echo Server on node n1
    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApp = echoServer.Install(nodes.Get(1));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Set up UDP Echo Client on node n0
    Address serverAddress;
    if (!useIpv6) {
        Ptr<Ipv4> ipv4_n1 = nodes.Get(1)->GetObject<Ipv4>();
        serverAddress = InetSocketAddress(ipv4_n1->GetAddress(1, 0).GetLocal(), port);
    } else {
        Ptr<Ipv6> ipv6_n1 = nodes.Get(1)->GetObject<Ipv6>();
        serverAddress = Inet6SocketAddress(ipv6_n1->GetAddress(1, 0).GetLocal(), port);
    }

    UdpEchoClientHelper echoClient(serverAddress);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = echoClient.Install(nodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Setup tracing
    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("udp-echo.tr");

    for (uint32_t i = 0; i < 4; ++i) {
        for (uint32_t j = i + 1; j < 4; ++j) {
            p2p.EnableAsciiAll(stream);
        }
    }

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}