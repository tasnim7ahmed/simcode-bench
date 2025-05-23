#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/udp-echo-helper.h"
#include "ns3/ipv4-global-routing-helper.h"

int main()
{
    ns3::Time::SetResolution(ns3::Time::NS);
    // ns3::LogComponentEnable("UdpEchoClientApplication", ns3::LOG_LEVEL_INFO);
    // ns3::LogComponentEnable("UdpEchoServerApplication", ns3::LOG_LEVEL_INFO);

    ns3::NodeContainer nodes;
    nodes.Create(4); // Four nodes

    ns3::CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", ns3::StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", ns3::TimeValue(ns3::NanoSeconds(6560)));

    ns3::NetDeviceContainer devices = csma.Install(nodes);

    ns3::InternetStackHelper internet;
    internet.Install(nodes);

    ns3::Ipv4AddressHelper ipv4;
    ipv4.SetBase("192.168.1.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // UDP Echo Server on the last node (index 3)
    ns3::UdpEchoServerHelper echoServer(9); // Port 9
    ns3::ApplicationContainer serverApps = echoServer.Install(nodes.Get(3));
    serverApps.Start(ns3::Seconds(1.0));
    serverApps.Stop(ns3::Seconds(10.0));

    // UDP Echo Client on the first node (index 0)
    // Server address is the IPv4 address of the last node's interface
    ns3::UdpEchoClientHelper echoClient(interfaces.GetAddress(3), 9); // Server IP and port
    echoClient.SetAttribute("MaxPackets", ns3::UintegerValue(5));     // Send 5 packets
    echoClient.SetAttribute("Interval", ns3::TimeValue(ns3::Seconds(1.0))); // 1 packet per second
    echoClient.SetAttribute("PacketSize", ns3::UintegerValue(512));   // 512 bytes per packet

    ns3::ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(ns3::Seconds(2.0)); // Start client after server
    clientApps.Stop(ns3::Seconds(10.0));

    ns3::Simulator::Stop(ns3::Seconds(10.0)); // Simulation runs for 10 seconds
    ns3::Simulator::Run();
    ns3::Simulator::Destroy();

    return 0;
}