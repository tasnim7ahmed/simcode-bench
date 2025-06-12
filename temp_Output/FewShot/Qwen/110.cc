#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-static-routing-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging for Ping and Icmpv6
    LogComponentEnable("Ping", LOG_LEVEL_INFO);
    LogComponentEnable("Icmpv6L4Protocol", LOG_LEVEL_INFO);

    // Create two nodes (n0 and n1)
    NodeContainer nodes;
    nodes.Create(2);

    // Configure CSMA channel
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("5Mbps"));
    csma.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices = csma.Install(nodes);

    // Install the IPv6 stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IPv6 addresses
    Ipv6AddressHelper address;
    address.SetBase(Ipv6Address("2001:db8::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer interfaces = address.Assign(devices);

    // Set up tracing of queues and packet receptions
    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("ping6.tr");
    csma.EnableAsciiAll(stream);

    // Set up Ping application from n0 to n1
    uint32_t maxPings = 5;
    PingHelper ping(interfaces.GetAddress(1, 1)); // IPv6 address of n1
    ping.SetAttribute("Count", UintegerValue(maxPings));
    ping.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    ping.SetAttribute("Size", UintegerValue(64));

    ApplicationContainer clientApp = ping.Install(nodes.Get(0));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(10.0));

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}