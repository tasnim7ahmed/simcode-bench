#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

// NS_LOG_COMPONENT_DEFINE("Ipv6PingP2p"); // Uncomment for logging if needed

int main(int argc, char *argv[])
{
    // Set default time resolution
    ns3::Time::SetResolution(ns3::Time::NS);

    // Create two nodes
    ns3::NodeContainer nodes;
    nodes.Create(2);

    // Configure Point-to-Point link
    ns3::PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", ns3::StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", ns3::StringValue("10ms"));

    ns3::NetDeviceContainer devices;
    devices = p2p.Install(nodes);

    // Install Internet Stack (IPv4 and IPv6)
    ns3::InternetStackHelper internet;
    internet.Install(nodes);

    // Assign IPv6 addresses
    ns3::Ipv6AddressHelper ipv6;
    ipv6.SetBase(ns3::Ipv6Address("2001:1::"), ns3::Ipv6Prefix64());
    ns3::Ipv6InterfaceContainer ipv6Interfaces;
    ipv6Interfaces = ipv6.Assign(devices);

    // Get the IPv6 address of node 1 (server)
    ns3::Ipv6Address serverAddress = ipv6Interfaces.GetAddress(1, 0); // Interface 0 on node 1, 0th address

    // Create ICMPv6 Echo application (ping client)
    // Node 0 (client) sends ping to Node 1 (server)
    ns3::V6PingHelper ping;
    ping.SetTarget(serverAddress);
    ping.SetAttribute("StartTime", ns3::TimeValue(ns3::Seconds(1.0)));
    ping.SetAttribute("StopTime", ns3::TimeValue(ns3::Seconds(9.0))); // Stop before simulation end

    ns3::ApplicationContainer apps = ping.Install(nodes.Get(0)); // Install on node 0 (client)

    // Enable pcap tracing
    p2p.EnablePcapAll("ipv6-ping-p2p");

    // Set simulation stop time
    ns3::Simulator::Stop(ns3::Seconds(10.0));

    // Run the simulation
    ns3::Simulator::Run();

    // Clean up
    ns3::Simulator::Destroy();

    return 0;
}