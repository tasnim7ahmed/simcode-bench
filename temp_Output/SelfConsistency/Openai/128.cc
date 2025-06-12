/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Network Topology:
 *
 *   n0 ---- n1
 *    |       |
 *   n2 ---- n3
 *
 * Four routers interconnected. OSPFv2 Link State Routing is simulated.
 * Traffic is generated from n0 to n3. The simulation traces OSPF LSA exchanges
 * and generates pcap files for visualization.
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ospf-helper.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("OspfLsaSimulation");

int
main(int argc, char *argv[])
{
    // Log settings
    LogComponentEnable("OspfLsaSimulation", LOG_LEVEL_INFO);

    // Command line args, if any
    CommandLine cmd;
    cmd.Parse(argc, argv);

    // 1. Create 4 nodes
    NodeContainer routers;
    routers.Create(4);

    // 2. Connect the nodes with point-to-point links as per the topology
    // links: n0-n1, n1-n3, n3-n2, n2-n0
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer d0d1 = p2p.Install(routers.Get(0), routers.Get(1));
    NetDeviceContainer d1d3 = p2p.Install(routers.Get(1), routers.Get(3));
    NetDeviceContainer d3d2 = p2p.Install(routers.Get(3), routers.Get(2));
    NetDeviceContainer d2d0 = p2p.Install(routers.Get(2), routers.Get(0));

    // 3. Install Internet stack with OSPF
    OspfHelper ospfRouting;
    Ipv4ListRoutingHelper listRouting;
    listRouting.Add(ospfRouting, 10); // OSPF priority high
    InternetStackHelper internet;
    internet.SetRoutingHelper(listRouting);
    internet.Install(routers);

    // 4. Assign IP addresses
    Ipv4AddressHelper ipv4;
    std::vector<Ipv4InterfaceContainer> interfaces;

    ipv4.SetBase("10.0.1.0", "255.255.255.0");
    interfaces.push_back(ipv4.Assign(d0d1));

    ipv4.SetBase("10.0.2.0", "255.255.255.0");
    interfaces.push_back(ipv4.Assign(d1d3));

    ipv4.SetBase("10.0.3.0", "255.255.255.0");
    interfaces.push_back(ipv4.Assign(d3d2));

    ipv4.SetBase("10.0.4.0", "255.255.255.0");
    interfaces.push_back(ipv4.Assign(d2d0));

    // 5. Enable OSPF LSA tracing (packet tracing via pcap)
    p2p.EnablePcapAll("ospf-lsa", true);

    // 6. Generate traffic: UDP from n0 to n3
    uint16_t port = 9000;
    // Install server on n3
    UdpServerHelper server(port);
    ApplicationContainer serverApp = server.Install(routers.Get(3));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(20.0));

    // Install client on n0; send to n3's IP on link n3-n2 (for uniqueness)
    Ipv4Address destAddr = interfaces[2].GetAddress(0); // n3's side of n3-n2
    UdpClientHelper client(destAddr, port);
    client.SetAttribute("MaxPackets", UintegerValue(1000));
    client.SetAttribute("Interval", TimeValue(MilliSeconds(50)));
    client.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApp = client.Install(routers.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(20.0));

    // 7. Simulate a network change: bring down link between n2 and n0 at 7s
    Simulator::Schedule(Seconds(7.0), [&](){
        NS_LOG_INFO("Simulated link failure: n2 <-> n0 going DOWN at 7s");
        d2d0.Get(0)->SetDown(); // n2
        d2d0.Get(1)->SetDown(); // n0
    });

    // 8. Bring the link back up at 13s
    Simulator::Schedule(Seconds(13.0), [&](){
        NS_LOG_INFO("Simulated link recovery: n2 <-> n0 going UP at 13s");
        d2d0.Get(0)->SetUp();
        d2d0.Get(1)->SetUp();
    });

    // 9. Run simulation
    Simulator::Stop(Seconds(21.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}