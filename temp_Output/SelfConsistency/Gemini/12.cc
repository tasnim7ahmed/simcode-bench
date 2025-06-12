#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("CsmaUdpTraceFileExample");

int main(int argc, char* argv[]) {
    bool verbose = false;
    bool ipv6 = false;

    CommandLine cmd;
    cmd.AddValue("verbose", "Tell application containers to log if true", verbose);
    cmd.AddValue("ipv6", "Use IPv6 if true", ipv6);
    cmd.Parse(argc, argv);

    // Set up logging based on command-line argument
    if (verbose) {
        LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
        LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
    }

    // Create nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Create CSMA channel
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(DataRate("5Mbps")));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
    csma.SetDeviceAttribute("Mtu", UintegerValue(1500));

    NetDeviceContainer devices = csma.Install(nodes);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    Ipv6AddressHelper ipv6Address;
    ipv6Address.SetBase("2001:db8::", Ipv6Prefix(64));
    Ipv6InterfaceContainer ipv6Interfaces = ipv6Address.Assign(devices);

    if (ipv6) {
        ipv6Interfaces.GetAddress(0, 1);
        ipv6Interfaces.GetAddress(1, 1);
    }

    // Create UDP server
    UdpServerHelper server(4000);
    ApplicationContainer serverApps = server.Install(nodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Create UDP client
    UdpClientHelper client(interfaces.GetAddress(1), 4000);
    client.SetAttribute("MaxPackets", UintegerValue(4294967295));
    client.SetAttribute("Interval", TimeValue(MilliSeconds(1)));
    client.SetAttribute("PacketSize", UintegerValue(1472)); // MTU - IP header (20) - UDP header (8)
    ApplicationContainer clientApps = client.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Simulator::Stop(Seconds(10.0));

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}