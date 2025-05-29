#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/ipv4-routing-helper.h"
#include "ns3/ospf-helper.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Configure simulation time
    double simTime = 20.0;

    // Enable logging if needed
    // LogComponentEnable ("OspfRouter", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(4);

    // Create point-to-point links: 0<->1, 1<->2, 1<->3, 2<->3
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer d01 = p2p.Install(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer d12 = p2p.Install(nodes.Get(1), nodes.Get(2));
    NetDeviceContainer d13 = p2p.Install(nodes.Get(1), nodes.Get(3));
    NetDeviceContainer d23 = p2p.Install(nodes.Get(2), nodes.Get(3));

    // Install Internet stack with OSPF
    OspfHelper ospf;
    InternetStackHelper internet;
    internet.SetRoutingHelper(ospf);
    internet.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i01 = ipv4.Assign(d01);
    ipv4.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer i12 = ipv4.Assign(d12);
    ipv4.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer i13 = ipv4.Assign(d13);
    ipv4.SetBase("10.1.4.0", "255.255.255.0");
    Ipv4InterfaceContainer i23 = ipv4.Assign(d23);

    // OSPF interface configuration (optional, default area 0)
    for (uint32_t i = 0; i < nodes.GetN(); ++i)
    {
        Ptr<Ipv4> ipv4 = nodes.Get(i)->GetObject<Ipv4>();
        for (uint32_t j = 1; j < ipv4->GetNInterfaces(); j++)
        {
            // All interfaces are set to OSPF area 0 (by default)
            ospf.SetOspfInterfaceMetric(nodes.Get(i), j, 10);
        }
    }

    // Set up UDP flow from node 0 to node 3
    uint16_t port = 5000;
    UdpServerHelper server(port);
    ApplicationContainer serverApps = server.Install(nodes.Get(3));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(simTime));

    UdpClientHelper client(i13.GetAddress(1), port);
    client.SetAttribute("MaxPackets", UintegerValue(1000));
    client.SetAttribute("Interval", TimeValue(MilliSeconds(50)));
    client.SetAttribute("PacketSize", UintegerValue(512));
    ApplicationContainer clientApps = client.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(simTime));

    // Set up NetAnim
    AnimationInterface anim("ospf-netanim.xml");

    // Set node positions for NetAnim (simple layout)
    anim.SetConstantPosition(nodes.Get(0), 50.0, 50.0);
    anim.SetConstantPosition(nodes.Get(1), 150.0, 100.0);
    anim.SetConstantPosition(nodes.Get(2), 250.0, 50.0);
    anim.SetConstantPosition(nodes.Get(3), 250.0, 150.0);

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}