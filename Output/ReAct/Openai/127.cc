#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Enable OSPF
    CommandLine cmd;
    cmd.Parse(argc, argv);

    // Create four router nodes
    NodeContainer routers;
    routers.Create(4);

    // Create point-to-point links between routers (full mesh for demonstration)
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Link 0-1
    NetDeviceContainer d01 = p2p.Install(routers.Get(0), routers.Get(1));
    // Link 1-2
    NetDeviceContainer d12 = p2p.Install(routers.Get(1), routers.Get(2));
    // Link 2-3
    NetDeviceContainer d23 = p2p.Install(routers.Get(2), routers.Get(3));
    // Link 3-0
    NetDeviceContainer d30 = p2p.Install(routers.Get(3), routers.Get(0));
    // Link 1-3
    NetDeviceContainer d13 = p2p.Install(routers.Get(1), routers.Get(3));

    // Install Internet stack and enable OSPF routing
    OspfHelper ospf;
    Ipv4ListRoutingHelper list;
    list.Add(ospf, 10);
    InternetStackHelper stack;
    stack.SetRoutingHelper(list);
    stack.Install(routers);

    // Assign IP addresses
    Ipv4AddressHelper addr;

    addr.SetBase("10.0.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i01 = addr.Assign(d01);
    addr.SetBase("10.0.2.0", "255.255.255.0");
    Ipv4InterfaceContainer i12 = addr.Assign(d12);
    addr.SetBase("10.0.3.0", "255.255.255.0");
    Ipv4InterfaceContainer i23 = addr.Assign(d23);
    addr.SetBase("10.0.4.0", "255.255.255.0");
    Ipv4InterfaceContainer i30 = addr.Assign(d30);
    addr.SetBase("10.0.5.0", "255.255.255.0");
    Ipv4InterfaceContainer i13 = addr.Assign(d13);

    // Traffic: UDP from Router 0 to Router 2
    uint16_t port = 5000;
    UdpServerHelper server(port);
    ApplicationContainer apps = server.Install(routers.Get(2));
    apps.Start(Seconds(1.0));
    apps.Stop(Seconds(9.0));

    UdpClientHelper client(i12.GetAddress(1), port);
    client.SetAttribute("MaxPackets", UintegerValue(20));
    client.SetAttribute("Interval", TimeValue(Seconds(0.5)));
    client.SetAttribute("PacketSize", UintegerValue(512));
    apps = client.Install(routers.Get(0));
    apps.Start(Seconds(2.0));
    apps.Stop(Seconds(9.0));

    // Enable pcap tracing
    p2p.EnablePcapAll("ospf-ls-routing");

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}