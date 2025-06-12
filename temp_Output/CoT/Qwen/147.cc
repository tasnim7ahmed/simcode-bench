#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("BranchTopologySimulation");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(3);

    PointToPointHelper p2p01;
    p2p01.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p01.SetChannelAttribute("Delay", StringValue("2ms"));

    PointToPointHelper p2p02;
    p2p02.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p02.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer dev01, dev02;
    dev01 = p2p01.Install(nodes.Get(0), nodes.Get(1));
    dev02 = p2p02.Install(nodes.Get(0), nodes.Get(2));

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address01;
    address01.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces01 = address01.Assign(dev01);

    Ipv4AddressHelper address02;
    address02.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces02 = address02.Assign(dev02);

    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient(interfaces01.GetAddress(1), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(10));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    p2p01.EnablePcapAll("branch_topology_link01");
    p2p02.EnablePcapAll("branch_topology_link02");

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}