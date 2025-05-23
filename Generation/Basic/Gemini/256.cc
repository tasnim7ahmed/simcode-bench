#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

// Do not include any using namespace directives in global scope

int main (int argc, char *argv[])
{
    // Enable logging for UDP Echo applications
    ns3::LogComponentEnable("UdpEchoClientApplication", ns3::LOG_LEVEL_INFO);
    ns3::LogComponentEnable("UdpEchoServerApplication", ns3::LOG_LEVEL_INFO);

    // 1. Create four nodes
    ns3::NodeContainer nodes;
    nodes.Create(4);

    // 2. Configure CSMA channel attributes
    ns3::CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", ns3::StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", ns3::TimeValue(ns3::NanoSeconds(6560)));

    // 3. Install CSMA devices on nodes
    ns3::NetDeviceContainer devices = csma.Install(nodes);

    // 4. Install Internet stack on all nodes
    ns3::InternetStackHelper internet;
    internet.Install(nodes);

    // 5. Assign IPv4 addresses
    ns3::Ipv4AddressHelper ipv4;
    ipv4.SetBase("192.168.1.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // 6. Configure UDP Echo Server on the last node (node 3)
    ns3::UdpEchoServerHelper echoServer(9); // Port 9
    ns3::ApplicationContainer serverApps = echoServer.Install(nodes.Get(3));
    serverApps.Start(ns3::Seconds(1.0)); // Server starts at 1 second
    serverApps.Stop(ns3::Seconds(10.0)); // Server stops at 10 seconds

    // 7. Configure UDP Echo Client on the first node (node 0)
    // Client targets the server's IP address (last node's IP) and port
    ns3::UdpEchoClientHelper echoClient(interfaces.GetAddress(3), 9);
    echoClient.SetAttribute("MaxPackets", ns3::UintegerValue(2));    // Send 2 packets
    echoClient.SetAttribute("Interval", ns3::TimeValue(ns3::Seconds(1.0))); // 1 second interval between packets
    echoClient.SetAttribute("PacketSize", ns3::UintegerValue(1024)); // 1024 bytes per packet

    ns3::ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(ns3::Seconds(2.0)); // Client starts at 2 seconds (after server is up)
    clientApps.Stop(ns3::Seconds(10.0));  // Client stops at 10 seconds

    // 8. Populate routing tables
    ns3::Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // 9. Set simulation stop time
    ns3::Simulator::Stop(ns3::Seconds(10.0));

    // 10. Run the simulation
    ns3::Simulator::Run();

    // 11. Clean up
    ns3::Simulator::Destroy();

    return 0;
}