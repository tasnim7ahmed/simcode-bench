#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("CsmaUdpSim");

int
main(int argc, char *argv[])
{
    bool useIpv6 = false;
    bool verbose = false;

    CommandLine cmd;
    cmd.AddValue("useIpv6", "Use IPv6 addressing (default: false=use IPv4)", useIpv6);
    cmd.AddValue("verbose", "Enable detailed logging", verbose);
    cmd.Parse(argc, argv);

    if (verbose)
    {
        LogComponentEnable("CsmaUdpSim", LOG_LEVEL_INFO);
        LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
        LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
    }

    NodeContainer nodes;
    nodes.Create(2);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(DataRate("5Mbps")));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
    csma.SetDeviceAttribute("Mtu", UintegerValue(1400));

    NetDeviceContainer devices = csma.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4Address serverIpv4;
    Ipv6Address serverIpv6;
    uint16_t port = 4000;

    if (!useIpv6)
    {
        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer ifs = ipv4.Assign(devices);
        serverIpv4 = ifs.GetAddress(1);

        NS_LOG_INFO("Server IPv4 address: " << serverIpv4);

        UdpServerHelper server(port);
        ApplicationContainer serverApps = server.Install(nodes.Get(1));
        serverApps.Start(Seconds(1.0));
        serverApps.Stop(Seconds(10.0));

        UdpClientHelper client(serverIpv4, port);
        client.SetAttribute("MaxPackets", UintegerValue(320));
        client.SetAttribute("Interval", TimeValue(MilliSeconds(50)));
        client.SetAttribute("PacketSize", UintegerValue(1024));

        ApplicationContainer clientApps = client.Install(nodes.Get(0));
        clientApps.Start(Seconds(2.0));
        clientApps.Stop(Seconds(10.0));
    }
    else
    {
        Ipv6AddressHelper ipv6;
        ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
        Ipv6InterfaceContainer ifs6 = ipv6.Assign(devices);
        ifs6.SetRouter(0, false);
        ifs6.SetRouter(1, false);
        serverIpv6 = ifs6.GetAddress(1,1); // Get the global address

        NS_LOG_INFO("Server IPv6 address: " << serverIpv6);

        UdpServerHelper server(port);
        ApplicationContainer serverApps = server.Install(nodes.Get(1));
        serverApps.Start(Seconds(1.0));
        serverApps.Stop(Seconds(10.0));

        UdpClientHelper client(serverIpv6, port);
        client.SetAttribute("MaxPackets", UintegerValue(320));
        client.SetAttribute("Interval", TimeValue(MilliSeconds(50)));
        client.SetAttribute("PacketSize", UintegerValue(1024));

        ApplicationContainer clientApps = client.Install(nodes.Get(0));
        clientApps.Start(Seconds(2.0));
        clientApps.Stop(Seconds(10.0));
    }

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}