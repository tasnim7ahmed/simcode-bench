#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("RingTopologySimulation");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(3);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices[3];
    devices[0] = p2p.Install(nodes.Get(0), nodes.Get(1));
    devices[1] = p2p.Install(nodes.Get(1), nodes.Get(2));
    devices[2] = p2p.Install(nodes.Get(2), nodes.Get(0));

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    Ipv4InterfaceContainer interfaces[3];

    address.SetBase("10.1.1.0", "255.255.255.0");
    interfaces[0] = address.Assign(devices[0]);

    address.SetBase("10.1.2.0", "255.255.255.0");
    interfaces[1] = address.Assign(devices[1]);

    address.SetBase("10.1.3.0", "255.255.255.0");
    interfaces[2] = address.Assign(devices[2]);

    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient(interfaces[0].GetAddress(1), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    AsciiTraceHelper ascii;
    p2p.EnableAsciiAll(ascii.CreateFileStream("ring-topology-simulation.tr"));
    p2p.EnablePcapAll("ring-topology-simulation");

    AnimationInterface anim("ring-topology.xml");
    anim.SetConstantPosition(nodes.Get(0), 0.0, 0.0);
    anim.SetConstantPosition(nodes.Get(1), 20.0, 0.0);
    anim.SetConstantPosition(nodes.Get(2), 10.0, 17.32);

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}