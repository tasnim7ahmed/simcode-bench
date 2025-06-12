#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(4);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices1 = p2p.Install(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer devices2 = p2p.Install(nodes.Get(0), nodes.Get(2));
    NetDeviceContainer devices3 = p2p.Install(nodes.Get(0), nodes.Get(3));

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces1 = address.Assign(devices1);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces2 = address.Assign(devices2);

    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces3 = address.Assign(devices3);

    UdpEchoClientHelper echoClient1(interfaces1.GetAddress(1), 9);
    echoClient1.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient1.SetAttribute("MaxPackets", UintegerValue(10));
    ApplicationContainer clientApps1 = echoClient1.Install(nodes.Get(1));
    clientApps1.Start(Seconds(1.0));
    clientApps1.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient2(interfaces2.GetAddress(1), 9);
    echoClient2.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient2.SetAttribute("MaxPackets", UintegerValue(10));
    ApplicationContainer clientApps2 = echoClient2.Install(nodes.Get(2));
    clientApps2.Start(Seconds(1.5));
    clientApps2.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient3(interfaces3.GetAddress(1), 9);
    echoClient3.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient3.SetAttribute("MaxPackets", UintegerValue(10));
    ApplicationContainer clientApps3 = echoClient3.Install(nodes.Get(3));
    clientApps3.Start(Seconds(2.0));
    clientApps3.Stop(Seconds(10.0));

    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(0));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(10.0));

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    p2p.EnablePcapAll("branch");

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}