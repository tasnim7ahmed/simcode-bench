#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int
main (int argc, char *argv[])
{
    Time::SetResolution (Time::NS);
    LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create nodes: 3 routers (r0, r1, r2) and 4 subnet nodes (h0, h1, h2, h3)
    NodeContainer routers;
    routers.Create (3);

    NodeContainer subnetNodes;
    subnetNodes.Create (4);

    // Subnet 0: h0 <-> r0
    NodeContainer subnet0 (subnetNodes.Get(0), routers.Get(0));
    // Subnet 1: h1 <-> r0
    NodeContainer subnet1 (subnetNodes.Get(1), routers.Get(0));
    // Subnet 2: h2 <-> r1
    NodeContainer subnet2 (subnetNodes.Get(2), routers.Get(1));
    // Subnet 3: h3 <-> r2
    NodeContainer subnet3 (subnetNodes.Get(3), routers.Get(2));

    // Router interconnect: r0<->r1, r1<->r2, r0<->r2
    NodeContainer r0r1 (routers.Get(0), routers.Get(1));
    NodeContainer r1r2 (routers.Get(1), routers.Get(2));
    NodeContainer r0r2 (routers.Get(0), routers.Get(2));

    // Install Internet stack with RIPng (Distance Vector Routing) enabled
    RipHelper ripRouting;
    Ipv4ListRoutingHelper listRH;
    listRH.Add (ripRouting, 0);

    InternetStackHelper internet;
    internet.SetRoutingHelper (listRH); // Master routing helper
    internet.Install (routers);
    internet.Install (subnetNodes);

    // Point-to-point config
    PointToPointHelper p2p_subnet;
    p2p_subnet.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
    p2p_subnet.SetChannelAttribute ("Delay", StringValue ("2ms"));

    PointToPointHelper p2p_rtr;
    p2p_rtr.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
    p2p_rtr.SetChannelAttribute ("Delay", StringValue ("1ms"));

    // NetDevice containers
    NetDeviceContainer d_subnet0 = p2p_subnet.Install (subnet0);
    NetDeviceContainer d_subnet1 = p2p_subnet.Install (subnet1);
    NetDeviceContainer d_subnet2 = p2p_subnet.Install (subnet2);
    NetDeviceContainer d_subnet3 = p2p_subnet.Install (subnet3);

    NetDeviceContainer d_r0r1 = p2p_rtr.Install (r0r1);
    NetDeviceContainer d_r1r2 = p2p_rtr.Install (r1r2);
    NetDeviceContainer d_r0r2 = p2p_rtr.Install (r0r2);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    std::vector<Ipv4InterfaceContainer> subnets;

    ipv4.SetBase ("10.0.1.0", "255.255.255.0");
    subnets.push_back (ipv4.Assign (d_subnet0));

    ipv4.SetBase ("10.0.2.0", "255.255.255.0");
    subnets.push_back (ipv4.Assign (d_subnet1));

    ipv4.SetBase ("10.0.3.0", "255.255.255.0");
    subnets.push_back (ipv4.Assign (d_subnet2));

    ipv4.SetBase ("10.0.4.0", "255.255.255.0");
    subnets.push_back (ipv4.Assign (d_subnet3));

    ipv4.SetBase ("10.0.10.0", "255.255.255.0");
    Ipv4InterfaceContainer i_r0r1 = ipv4.Assign (d_r0r1);

    ipv4.SetBase ("10.0.11.0", "255.255.255.0");
    Ipv4InterfaceContainer i_r1r2 = ipv4.Assign (d_r1r2);

    ipv4.SetBase ("10.0.12.0", "255.255.255.0");
    Ipv4InterfaceContainer i_r0r2 = ipv4.Assign (d_r0r2);

    // Enable routing tables printing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

    // UDP Traffic: h0 -> h3
    uint16_t echoPort = 9;
    UdpEchoServerHelper echoServer (echoPort);
    ApplicationContainer serverApps = echoServer.Install (subnetNodes.Get(3));
    serverApps.Start (Seconds (1.0));
    serverApps.Stop (Seconds (30.0));

    UdpEchoClientHelper echoClient (subnets[3].GetAddress (0), echoPort);
    echoClient.SetAttribute ("MaxPackets", UintegerValue (10));
    echoClient.SetAttribute ("Interval", TimeValue (Seconds (2.0)));
    echoClient.SetAttribute ("PacketSize", UintegerValue (64));

    ApplicationContainer clientApps = echoClient.Install (subnetNodes.Get(0));
    clientApps.Start (Seconds (2.0));
    clientApps.Stop (Seconds (30.0));

    // Enable pcap tracing
    p2p_subnet.EnablePcapAll ("dvr-subnet", false);
    p2p_rtr.EnablePcapAll ("dvr-router", false);

    // Capture all interfaces on routers for routing exchanges too
    for (uint32_t i = 0; i < routers.GetN (); ++i)
    {
        Ptr<Node> router = routers.Get (i);
        for (uint32_t j = 0; j < router->GetNDevices (); ++j)
        {
            Ptr<NetDevice> dev = router->GetDevice (j);
            std::ostringstream oss;
            oss << "router-" << i << "-dev-" << j << ".pcap";
            dev->TraceConnect ("PhyTxEnd", oss.str (), MakeCallback (&PcapHelper::PcapTraceSink));
        }
    }

    Simulator::Stop (Seconds (35.0));
    Simulator::Run ();
    Simulator::Destroy ();
    return 0;
}