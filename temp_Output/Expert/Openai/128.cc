#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/ospf-routing-helper.h"

using namespace ns3;

int
main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);

    // Create four router nodes
    NodeContainer routers;
    routers.Create(4);

    // Install Internet stack with OSPF enabled
    OspfRoutingHelper ospfHelper;
    InternetStackHelper stack;
    stack.SetRoutingHelper(ospfHelper);
    stack.Install(routers);

    // Create point-to-point links between routers (full mesh)
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer d01 = p2p.Install(routers.Get(0), routers.Get(1));
    NetDeviceContainer d02 = p2p.Install(routers.Get(0), routers.Get(2));
    NetDeviceContainer d03 = p2p.Install(routers.Get(0), routers.Get(3));
    NetDeviceContainer d12 = p2p.Install(routers.Get(1), routers.Get(2));
    NetDeviceContainer d13 = p2p.Install(routers.Get(1), routers.Get(3));
    NetDeviceContainer d23 = p2p.Install(routers.Get(2), routers.Get(3));

    // Assign IP addresses to each interface
    Ipv4AddressHelper ipv4;
    std::vector<Ipv4InterfaceContainer> ifaces(6);

    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    ifaces[0] = ipv4.Assign(d01);

    ipv4.SetBase("10.1.2.0", "255.255.255.0");
    ifaces[1] = ipv4.Assign(d02);

    ipv4.SetBase("10.1.3.0", "255.255.255.0");
    ifaces[2] = ipv4.Assign(d03);

    ipv4.SetBase("10.1.4.0", "255.255.255.0");
    ifaces[3] = ipv4.Assign(d12);

    ipv4.SetBase("10.1.5.0", "255.255.255.0");
    ifaces[4] = ipv4.Assign(d13);

    ipv4.SetBase("10.1.6.0", "255.255.255.0");
    ifaces[5] = ipv4.Assign(d23);

    // Enable OSPF globally
    ospfHelper.EnableOspf();

    // Enable PCAP tracing on all router devices (to observe OSPF/LSA)
    p2p.EnablePcapAll("ospf-lsa");

    // Install applications: UDP traffic from Router 0 to Router 3
    uint16_t port = 50000;

    // Packet sink on node 3
    Address sinkAddress(InetSocketAddress(ifaces[2].GetAddress(1), port));
    PacketSinkHelper sinkHelper("ns3::UdpSocketFactory", sinkAddress);
    ApplicationContainer sinkApp = sinkHelper.Install(routers.Get(3));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(30.0));

    // UDP client on node 0
    UdpClientHelper client(ifaces[2].GetAddress(1), port);
    client.SetAttribute("MaxPackets", UintegerValue(1000));
    client.SetAttribute("Interval", TimeValue(MilliSeconds(50)));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = client.Install(routers.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(28.0));

    // Simulate a link failure to trigger OSPF LSA
    Simulator::Schedule(Seconds(10.0), [&](){
        Ptr<NetDevice> nd = routers.Get(1)->GetDevice(2); // Device to router 3
        nd->SetReceiveCallback(MakeNullCallback<bool, Ptr<NetDevice>, Ptr<const Packet>, uint16_t, const Address &>());
        nd->SetPromiscReceiveCallback(MakeNullCallback<bool, Ptr<NetDevice>, Ptr<const Packet>, uint16_t, const Address &, const Address &, enum NetDevice::PacketType>());
        nd->SetMtu(0);
    });

    Simulator::Stop(Seconds(30.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}