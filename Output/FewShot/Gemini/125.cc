#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/rip.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);

    NodeContainer routers;
    routers.Create(5);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices1 = p2p.Install(NodeContainer(routers.Get(0), routers.Get(1)));
    NetDeviceContainer devices2 = p2p.Install(NodeContainer(routers.Get(0), routers.Get(2)));
    NetDeviceContainer devices3 = p2p.Install(NodeContainer(routers.Get(0), routers.Get(3)));
    NetDeviceContainer devices4 = p2p.Install(NodeContainer(routers.Get(0), routers.Get(4)));

    InternetStackHelper internet;
    RipHelper ripRouting;
    internet.SetRoutingHelper(ripRouting);
    internet.Install(routers);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i1 = ipv4.Assign(devices1);

    ipv4.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer i2 = ipv4.Assign(devices2);

    ipv4.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer i3 = ipv4.Assign(devices3);

    ipv4.SetBase("10.1.4.0", "255.255.255.0");
    Ipv4InterfaceContainer i4 = ipv4.Assign(devices4);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Create Applications (e.g., UDP echo client/server)
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApp = echoServer.Install(routers.Get(4));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient(i4.GetAddress(1), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(10));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = echoClient.Install(routers.Get(1));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Tracing
    AsciiTraceHelper ascii;
    p2p.EnableAsciiAll( "rip-star-topology");
    p2p.EnablePcapAll( "rip-star-topology");

    AnimationInterface anim("rip-star-topology.xml");
    anim.SetConstantPosition(routers.Get(0), 10, 10);
    anim.SetConstantPosition(routers.Get(1), 20, 20);
    anim.SetConstantPosition(routers.Get(2), 20, 0);
    anim.SetConstantPosition(routers.Get(3), 0, 0);
    anim.SetConstantPosition(routers.Get(4), 0, 20);

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}