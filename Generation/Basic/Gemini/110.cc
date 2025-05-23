#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/ping6-helper.h"
#include "ns3/ipv6-address-helper.h"

int main(int argc, char *argv[])
{
    // Enable logging for Ping6 and CSMA
    LogComponentEnable("Ping6Application", LOG_LEVEL_INFO);
    LogComponentEnable("CsmaNetDevice", LOG_LEVEL_INFO);

    // Create two nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Configure CSMA channel properties
    CsmaHelper csma;
    csma.SetQueue("ns3::DropTailQueue"); // Use DropTail queues
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", StringValue("2ms"));

    // Install CSMA devices on the nodes
    NetDeviceContainer devices = csma.Install(nodes);

    // Install the IPv6 Internet stack on the nodes
    InternetStackHelper internetv6;
    internetv6.Install(nodes);

    // Assign IPv6 addresses to the devices
    Ipv6AddressHelper ipv6;
    ipv6.SetBase(Ipv6Address("2001:db8::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer interfaces = ipv6.Assign(devices);

    // Get the global IPv6 address of n1 (node 1) for the Ping6 target
    // interface 0 is for node 0, interface 1 for node 1.
    // Address index 0 is link-local, index 1 is the global address.
    Ipv6Address n1Ipv6Address = interfaces.GetAddress(1, 1); 

    // Create a Ping6 application on n0 (node 0)
    Ping6Helper ping6;
    ping6.SetRemote(n1Ipv6Address);
    ping6.SetAttribute("MaxPackets", UintegerValue(3)); // Send 3 echo requests
    ping6.SetAttribute("Interval", TimeValue(Seconds(1.0))); // 1 second interval
    ping6.SetAttribute("PacketSize", UintegerValue(1024)); // 1024 byte packets

    ApplicationContainer pingApps = ping6.Install(nodes.Get(0)); // Install on n0
    pingApps.Start(Seconds(1.0)); // Start pinging after 1 second
    pingApps.Stop(Seconds(10.0)); // Stop pinging after 10 seconds

    // Enable ASCII tracing for CSMA devices (queues and packet receptions)
    AsciiTraceHelper ascii;
    csma.EnableAsciiAll(ascii.CreateFileStream("ping6.tr"));

    // Set simulation stop time
    Simulator::Stop(Seconds(11.0));

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}