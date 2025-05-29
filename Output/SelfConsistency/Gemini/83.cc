#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv6-address.h"
#include "ns3/network-module.h"
#include "ns3/on-off-application.h"
#include "ns3/packet-sink.h"
#include "ns3/uinteger.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("CsmaUdpExample");

int main(int argc, char* argv[]) {
    bool verbose = false;
    bool useIpv6 = false;

    CommandLine cmd;
    cmd.AddValue("verbose", "Tell application to log if true.", verbose);
    cmd.AddValue("useIpv6", "Use IPv6 addressing if true.", useIpv6);
    cmd.Parse(argc, argv);

    if (verbose) {
        LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
        LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
    }

    NodeContainer nodes;
    nodes.Create(2);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(DataRate("5Mbps")));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

    NetDeviceContainer devices;
    devices = csma.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    Ipv6AddressHelper address6;
    Ipv4InterfaceContainer interfaces;
    Ipv6InterfaceContainer interfaces6;

    if (useIpv6) {
        address6.SetBase(Ipv6Address("2001:db8::"), Ipv6Prefix(64));
        interfaces6 = address6.Assign(devices);
    } else {
        address.SetBase("10.1.1.0", "255.255.255.0");
        interfaces = address.Assign(devices);
    }

    uint16_t port = 9;  // Discard port (RFC 863)

    UdpServerHelper server(port);
    ApplicationContainer serverApps = server.Install(nodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    OnOffHelper client("ns3::UdpSocketFactory",
                       Address(InetSocketAddress(useIpv6 ? interfaces6.GetAddress(1)
                                                          : interfaces.GetAddress(1),
                                                 port)));
    client.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    client.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    client.SetAttribute("PacketSize", UintegerValue(1024));
    client.SetAttribute("DataRate", DataRateValue(DataRate("160kbps")));
    client.SetAttribute("MaxPackets", UintegerValue(320)); // Stop after sending 320 packets.

    ApplicationContainer clientApps = client.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();
    Ipv6GlobalRoutingHelper::PopulateRoutingTables();

    Simulator::Stop(Seconds(10.0));

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}