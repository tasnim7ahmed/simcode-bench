#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/applications-module.h"
#include "ns3/ospf-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    CommandLine cmd;
    cmd.Parse(argc, argv);

    // Create 4 router nodes
    NodeContainer routers;
    routers.Create(4);

    // Create point-to-point links for a mesh topology
    // Topology: 0-1-2-3 in a line for simplicity, plus 0-2 and 1-3 diagonals
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Link matrix
    NetDeviceContainer d0_1 = p2p.Install(routers.Get(0), routers.Get(1));
    NetDeviceContainer d1_2 = p2p.Install(routers.Get(1), routers.Get(2));
    NetDeviceContainer d2_3 = p2p.Install(routers.Get(2), routers.Get(3));
    NetDeviceContainer d0_2 = p2p.Install(routers.Get(0), routers.Get(2));
    NetDeviceContainer d1_3 = p2p.Install(routers.Get(1), routers.Get(3));

    // Install OSPF Internet stack
    OspfHelper ospf;
    InternetStackHelper internet;
    internet.SetRoutingHelper(ospf);
    internet.Install(routers);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.0.1.0", "255.255.255.0");
    Ipv4InterfaceContainer if0_1 = ipv4.Assign(d0_1);
    ipv4.SetBase("10.0.2.0", "255.255.255.0");
    Ipv4InterfaceContainer if1_2 = ipv4.Assign(d1_2);
    ipv4.SetBase("10.0.3.0", "255.255.255.0");
    Ipv4InterfaceContainer if2_3 = ipv4.Assign(d2_3);
    ipv4.SetBase("10.0.4.0", "255.255.255.0");
    Ipv4InterfaceContainer if0_2 = ipv4.Assign(d0_2);
    ipv4.SetBase("10.0.5.0", "255.255.255.0");
    Ipv4InterfaceContainer if1_3 = ipv4.Assign(d1_3);

    // Enable pcap tracing
    p2p.EnablePcapAll("ospf-topology", true);

    // Application: OnOff traffic from node 0 to node 3
    uint16_t port = 5000;
    OnOffHelper onoff("ns3::TcpSocketFactory", 
                      InetSocketAddress(if2_3.GetAddress(1), port));
    onoff.SetAttribute("DataRate", StringValue("2Mbps"));
    onoff.SetAttribute("PacketSize", UintegerValue(1024));
    onoff.SetAttribute("StartTime", TimeValue(Seconds(2.0)));
    onoff.SetAttribute("StopTime", TimeValue(Seconds(8.0)));

    ApplicationContainer clientApps = onoff.Install(routers.Get(0));

    // Sink on node 3
    PacketSinkHelper sink("ns3::TcpSocketFactory",
                          InetSocketAddress(Ipv4Address::GetAny(), port));
    sink.Install(routers.Get(3));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}