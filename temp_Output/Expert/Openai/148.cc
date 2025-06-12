#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int
main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);
    NodeContainer centralNode;
    centralNode.Create(1);

    NodeContainer n1, n2, n3;
    n1.Add(centralNode.Get(0));
    n1.Create(1);
    n2.Add(centralNode.Get(0));
    n2.Create(1);
    n3.Add(centralNode.Get(0));
    n3.Create(1);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer d01 = p2p.Install(n1);
    NetDeviceContainer d02 = p2p.Install(n2);
    NetDeviceContainer d03 = p2p.Install(n3);

    InternetStackHelper stack;
    stack.Install(n1.Get(0));
    stack.Install(n1.Get(1));
    stack.Install(n2.Get(1));
    stack.Install(n3.Get(1));

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i01 = address.Assign(d01);
    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer i02 = address.Assign(d02);
    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer i03 = address.Assign(d03);

    uint16_t port = 4000;

    UdpServerHelper server(port);
    ApplicationContainer serverApps = server.Install(n1.Get(0));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(10.0));

    UdpClientHelper client1(i01.GetAddress(0), port);
    client1.SetAttribute("MaxPackets", UintegerValue(320));
    client1.SetAttribute("Interval", TimeValue(MilliSeconds(30)));
    client1.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps1 = client1.Install(n1.Get(1));
    clientApps1.Start(Seconds(1.0));
    clientApps1.Stop(Seconds(10.0));

    UdpClientHelper client2(i02.GetAddress(0), port);
    client2.SetAttribute("MaxPackets", UintegerValue(320));
    client2.SetAttribute("Interval", TimeValue(MilliSeconds(30)));
    client2.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps2 = client2.Install(n2.Get(1));
    clientApps2.Start(Seconds(1.0));
    clientApps2.Stop(Seconds(10.0));

    UdpClientHelper client3(i03.GetAddress(0), port);
    client3.SetAttribute("MaxPackets", UintegerValue(320));
    client3.SetAttribute("Interval", TimeValue(MilliSeconds(30)));
    client3.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps3 = client3.Install(n3.Get(1));
    clientApps3.Start(Seconds(1.0));
    clientApps3.Stop(Seconds(10.0));

    p2p.EnablePcapAll("branch-topology");

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}