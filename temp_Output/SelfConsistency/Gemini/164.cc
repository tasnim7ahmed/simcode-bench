#include <iostream>
#include <fstream>
#include <string>

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/ospf-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("OspfNetanim");

int main(int argc, char* argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    LogComponent::SetLogLevel(LOG_LEVEL_INFO);
    LogComponent::SetPrintPrefix(true);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(4);

    // Install Internet stack
    InternetStackHelper internet;
    OspfHelper ospf;
    internet.SetRoutingHelper(ospf);
    internet.Install(nodes);

    // Create point-to-point links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices12 = p2p.Install(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer devices13 = p2p.Install(nodes.Get(0), nodes.Get(2));
    NetDeviceContainer devices23 = p2p.Install(nodes.Get(1), nodes.Get(2));
    NetDeviceContainer devices34 = p2p.Install(nodes.Get(2), nodes.Get(3));

    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces12 = ipv4.Assign(devices12);

    ipv4.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces13 = ipv4.Assign(devices13);

    ipv4.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces23 = ipv4.Assign(devices23);

    ipv4.SetBase("10.1.4.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces34 = ipv4.Assign(devices34);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Create Applications (Traffic)
    uint16_t port = 9; // Discard port (RFC 863)

    UdpEchoServerHelper echoServer(port);

    ApplicationContainer serverApps = echoServer.Install(nodes.Get(3));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient(interfaces34.GetAddress(1), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(100));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(0.01)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Enable Tracing for PCAP files
    //p2p.EnablePcapAll("ospf-netanim");

    // Enable NetAnim
    AnimationInterface anim("ospf-netanim.xml");
    anim.SetMaxPktsPerTraceFile(999999999999);
    anim.UpdateNodeSize(0,0,5.0);

    // Run the simulation
    Simulator::Stop(Seconds(11.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}