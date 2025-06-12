#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-address-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("UdpCsmaSimulation");

int main(int argc, char *argv[])
{
    bool enableLogging = false;
    std::string protocol = "ipv4";

    CommandLine cmd(__FILE__);
    cmd.AddValue("logging", "Enable detailed logging", enableLogging);
    cmd.AddValue("protocol", "Protocol to use: ipv4 or ipv6", protocol);
    cmd.Parse(argc, argv);

    if (enableLogging)
    {
        LogComponentEnable("UdpCsmaSimulation", LOG_LEVEL_INFO);
        LogComponentEnable("UdpClientApplication", LOG_LEVEL_ALL);
        LogComponentEnable("UdpServerApplication", LOG_LEVEL_ALL);
    }

    NodeContainer nodes;
    nodes.Create(2);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(DataRate("5Mbps")));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
    csma.SetDeviceAttribute("Mtu", UintegerValue(1400));

    NetDeviceContainer devices = csma.Install(nodes);

    InternetStackHelper stack;
    if (protocol == "ipv4")
    {
        Ipv4AddressHelper address;
        address.SetBase("192.168.1.0", "255.255.255.0");
        stack.Install(nodes);
        Ipv4InterfaceContainer interfaces = address.Assign(devices);
    }
    else if (protocol == "ipv6")
    {
        Ipv6AddressHelper address;
        address.SetBase("2001:db8::", Ipv6Prefix(64));
        stack.Install(nodes);
        Ipv6InterfaceContainer interfaces = address.Assign(devices);
    }
    else
    {
        NS_FATAL_ERROR("Invalid protocol specified; must be 'ipv4' or 'ipv6'.");
    }

    uint16_t port = 4000;
    UdpServerHelper server(port);
    ApplicationContainer serverApps = server.Install(nodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpClientHelper client;
    if (protocol == "ipv4")
    {
        Ipv4Address remoteIp = static_cast<Ipv4Address>(nodes.Get(1)->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal());
        client.SetRemote(remoteIp, port);
    }
    else
    {
        Ipv6Address remoteIp = static_cast<Ipv6Address>(nodes.Get(1)->GetObject<Ipv6>()->GetAddress(1, 0).GetLocal());
        client.SetRemote(remoteIp, port);
    }

    client.SetAttribute("MaxPackets", UintegerValue(320));
    client.SetAttribute("Interval", TimeValue(MilliSeconds(50)));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = client.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}