#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    bool useIpv6 = false;
    bool enableLogging = false;
    std::string traceFile = "src/applications/examples/udp-trace-client.trace";

    CommandLine cmd(__FILE__);
    cmd.AddValue("useIpv6", "Use IPv6 addressing instead of IPv4", useIpv6);
    cmd.AddValue("enableLogging", "Enable detailed logging for UDP client/server", enableLogging);
    cmd.AddValue("traceFile", "Path to the UDP trace file for the client", traceFile);
    cmd.Parse(argc, argv);

    if (enableLogging)
    {
        LogComponentEnable("UdpTraceClient", LOG_LEVEL_INFO);
        LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
    }

    NodeContainer nodes;
    nodes.Create(2);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("5Mbps"));
    csma.SetChannelAttribute("Delay", StringValue("2ms"));
    csma.SetDeviceAttribute("Mtu", UintegerValue(1500));
    NetDeviceContainer devices = csma.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4InterfaceContainer ipv4Interfaces;
    Ipv6InterfaceContainer ipv6Interfaces;

    if (useIpv6)
    {
        Ipv6AddressHelper ipv6;
        ipv6.SetBase("2001:db8::", Ipv6Prefix(64));
        ipv6Interfaces = ipv6.Assign(devices);
    }
    else
    {
        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.1.1.0", "255.255.255.0");
        ipv4Interfaces = ipv4.Assign(devices);
    }

    uint16_t serverPort = 4000;
    UdpServerHelper server(serverPort);
    ApplicationContainer serverApps = server.Install(nodes.Get(1)); // n1
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    uint32_t maxPacketSize = 1500 - 20 - 8; // MTU - IP Header - UDP Header = 1472 bytes

    UdpTraceClientHelper client;
    if (useIpv6)
    {
        client.SetRemote(ipv6Interfaces.GetAddress(1), serverPort); // n1's IPv6 address
    }
    else
    {
        client.SetRemote(ipv4Interfaces.GetAddress(1), serverPort); // n1's IPv4 address
    }

    client.SetAttribute("MaxPacketSize", UintegerValue(maxPacketSize));
    client.SetAttribute("TraceFilename", StringValue(traceFile));
    ApplicationContainer clientApps = client.Install(nodes.Get(0)); // n0
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    Simulator::Stop(Seconds(11.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}