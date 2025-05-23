#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ospf-helper.h"

int main(int argc, char *argv[])
{
    // Enable logging for OSPF and applications to observe behavior
    ns3::LogComponentEnable("OspfProcess", ns3::LOG_LEVEL_INFO);
    ns3::LogComponentEnable("Ipv4L3Protocol", ns3::LOG_LEVEL_INFO); // For routing table changes
    ns3::LogComponentEnable("UdpEchoClientApplication", ns3::LOG_LEVEL_INFO);
    ns3::LogComponentEnable("UdpEchoServerApplication", ns3::LOG_LEVEL_INFO);

    // Allow pcap tracing to be enabled/disabled via command line
    bool tracing = true;
    ns3::CommandLine cmd(__FILE__);
    cmd.AddValue("tracing", "Enable pcap tracing (true/false)", tracing);
    cmd.Parse(argc, argv);

    // 1. Create four router nodes
    ns3::NodeContainer routers;
    routers.Create(4); // R0, R1, R2, R3

    // 2. Install Internet Stack with OSPF on all routers
    // OspfHelper will install the InternetStack and configure OSPF on all interfaces.
    ns3::OspfHelper ospf;
    ospf.Install(routers);

    // 3. Configure Point-to-Point Links between routers
    ns3::PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", ns3::StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", ns3::StringValue("10ms"));

    ns3::Ipv4AddressHelper address;

    // Link R0 <-> R1
    ns3::NetDeviceContainer devices01 = p2p.Install(routers.Get(0), routers.Get(1));
    address.SetBase("10.1.1.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer interfaces01 = address.Assign(devices01);

    // Link R1 <-> R2
    ns3::NetDeviceContainer devices12 = p2p.Install(routers.Get(1), routers.Get(2));
    address.SetBase("10.1.2.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer interfaces12 = address.Assign(devices12);

    // Link R2 <-> R3
    ns3::NetDeviceContainer devices23 = p2p.Install(routers.Get(2), routers.Get(3));
    address.SetBase("10.1.3.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer interfaces23 = address.Assign(devices23);

    // OSPF will dynamically discover routes among these interconnected routers.

    // 4. Traffic Generation: UDP Echo from R0 to R3
    // Get the IP address of R3 on the 10.1.3.0 network segment (connected to R2)
    // interfaces23.GetAddress(0) is R2's address on 10.1.3.0
    // interfaces23.GetAddress(1) is R3's address on 10.1.3.0
    ns3::Ipv4Address sinkAddress = interfaces23.GetAddress(1); 
    uint16_t port = 9; // Echo server port

    // Install UDP Echo Server on R3
    ns3::UdpEchoServerHelper echoServer(port);
    ns3::ApplicationContainer serverApps = echoServer.Install(routers.Get(3));
    serverApps.Start(ns3::Seconds(1.0)); // Start server at 1 second
    serverApps.Stop(ns3::Seconds(10.0)); // Stop server at 10 seconds

    // Install UDP Echo Client on R0, targeting R3
    ns3::UdpEchoClientHelper echoClient(sinkAddress, port);
    echoClient.SetAttribute("MaxPackets", ns3::UintegerValue(5));    // Send 5 packets
    echoClient.SetAttribute("Interval", ns3::TimeValue(ns3::Seconds(1.0))); // Send one packet per second
    echoClient.SetAttribute("PacketSize", ns3::UintegerValue(1024)); // Packet size of 1024 bytes

    ns3::ApplicationContainer clientApps = echoClient.Install(routers.Get(0));
    clientApps.Start(ns3::Seconds(2.0)); // Start client at 2 seconds (after server and OSPF convergence)
    clientApps.Stop(ns3::Seconds(10.0)); // Stop client at 10 seconds

    // 5. Enable Pcap Tracing
    if (tracing)
    {
        // This will generate pcap files for all point-to-point devices.
        // Files will be named like "ospf-routing-chain-0-0.pcap", "ospf-routing-chain-0-1.pcap", etc.
        p2p.EnablePcapAll("ospf-routing-chain");
    }

    // 6. Set simulation stop time and run
    ns3::Simulator::Stop(ns3::Seconds(11.0)); // Stop the simulation after 11 seconds
    ns3::Simulator::Run();
    ns3::Simulator::Destroy();

    return 0;
}