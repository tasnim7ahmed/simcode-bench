#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(3);

    NodeContainer n0n1 = NodeContainer(nodes.Get(0), nodes.Get(1));
    NodeContainer n0n2 = NodeContainer(nodes.Get(0), nodes.Get(2));

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer d0d1 = p2p.Install(n0n1);
    NetDeviceContainer d0d2 = p2p.Install(n0n2);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i0i1 = address.Assign(d0d1);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer i0i2 = address.Assign(d0d2);

    uint16_t port1 = 8000;
    uint16_t port2 = 8001;

    UdpServerHelper server1(port1);
    ApplicationContainer apps1 = server1.Install(nodes.Get(1));
    apps1.Start(Seconds(1.0));
    apps1.Stop(Seconds(10.0));

    UdpServerHelper server2(port2);
    ApplicationContainer apps2 = server2.Install(nodes.Get(2));
    apps2.Start(Seconds(1.0));
    apps2.Stop(Seconds(10.0));

    UdpClientHelper client1(i0i1.GetAddress(1), port1);
    client1.SetAttribute("MaxPackets", UintegerValue(320));
    client1.SetAttribute("Interval", TimeValue(MilliSeconds(30)));
    client1.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps1 = client1.Install(nodes.Get(0));
    clientApps1.Start(Seconds(2.0));
    clientApps1.Stop(Seconds(10.0));

    UdpClientHelper client2(i0i2.GetAddress(1), port2);
    client2.SetAttribute("MaxPackets", UintegerValue(320));
    client2.SetAttribute("Interval", TimeValue(MilliSeconds(30)));
    client2.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps2 = client2.Install(nodes.Get(0));
    clientApps2.Start(Seconds(2.5));
    clientApps2.Stop(Seconds(10.0));

    p2p.EnablePcapAll("branch-topology");

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}