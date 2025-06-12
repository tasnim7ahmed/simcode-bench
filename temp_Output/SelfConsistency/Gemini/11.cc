#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-address.h"
#include "ns3/ipv6-address.h"
#include "ns3/netanim-module.h"

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
    csma.SetMtu(1400);

    NetDeviceContainer devices = csma.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    Ipv6AddressHelper address6;
    address6.SetBase("2001:db8::", Ipv6Prefix(64));
    Ipv6InterfaceContainer interfaces6;
    if (useIpv6) {
        interfaces6 = address6.Assign(devices);
    }

    uint16_t port = 4000;
    UdpServerHelper server(port);
    ApplicationContainer serverApps = server.Install(nodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpClientHelper client(useIpv6 ? interfaces6.GetAddress(1) : interfaces.GetAddress(1), port);
    client.SetAttribute("MaxPackets", UintegerValue(320));
    client.SetAttribute("Interval", TimeValue(MilliSeconds(50)));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = client.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    if (useIpv6) {
        for (uint32_t i = 0; i < interfaces6.GetNInterfaces(); ++i) {
            Ptr<Ipv6> ipv6 = nodes.Get(i)->GetObject<Ipv6>();
            ipv6->SetForwarding(0, true);
            ipv6->SetDefaultRoute(0, interfaces6.GetAddress(1));
        }
    }

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}