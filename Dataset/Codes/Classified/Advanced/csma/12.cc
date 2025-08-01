// Network topology
//
//       n0    n1
//       |     |
//       =======
//         LAN (CSMA)
//
// - UDP flow from n0 to n1 of packets drawn from a trace file
//  -- option to use IPv4 or IPv6 addressing
//  -- option to disable logging statements

#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"

#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("UdpTraceClientServerExample");

int
main(int argc, char* argv[])
{
    // Declare variables used in command-line arguments
    bool useV6 = false;
    bool logging = true;
    Address serverAddress;

    CommandLine cmd(__FILE__);
    cmd.AddValue("useIpv6", "Use Ipv6", useV6);
    cmd.AddValue("logging", "Enable logging", logging);
    cmd.Parse(argc, argv);

    if (logging)
    {
        LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
        LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
    }

    NS_LOG_INFO("Create nodes in above topology.");
    NodeContainer n;
    n.Create(2);

    InternetStackHelper internet;
    internet.Install(n);

    NS_LOG_INFO("Create channel between the two nodes.");
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(DataRate(5000000)));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
    csma.SetDeviceAttribute("Mtu", UintegerValue(1500));
    NetDeviceContainer d = csma.Install(n);

    NS_LOG_INFO("Assign IP Addresses.");
    if (!useV6)
    {
        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer i = ipv4.Assign(d);
        serverAddress = Address(i.GetAddress(1));
    }
    else
    {
        Ipv6AddressHelper ipv6;
        ipv6.SetBase("2001:0000:f00d:cafe::", Ipv6Prefix(64));
        Ipv6InterfaceContainer i6 = ipv6.Assign(d);
        serverAddress = Address(i6.GetAddress(1, 1));
    }

    NS_LOG_INFO("Create UdpServer application on node 1.");
    uint16_t port = 4000;
    UdpServerHelper server(port);
    ApplicationContainer apps = server.Install(n.Get(1));
    apps.Start(Seconds(1.0));
    apps.Stop(Seconds(10.0));

    NS_LOG_INFO("Create UdpClient application on node 0 to send to node 1.");
    uint32_t MaxPacketSize = 1472; // Back off 20 (IP) + 8 (UDP) bytes from MTU
    UdpTraceClientHelper client(serverAddress, port, "");
    client.SetAttribute("MaxPacketSize", UintegerValue(MaxPacketSize));
    apps = client.Install(n.Get(0));
    apps.Start(Seconds(2.0));
    apps.Stop(Seconds(10.0));

    NS_LOG_INFO("Run Simulation.");
    Simulator::Run();
    Simulator::Destroy();
    NS_LOG_INFO("Done.");

    return 0;
}
