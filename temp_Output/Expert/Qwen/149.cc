#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("LineTopologyUdp");

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

    PointToPointHelper p2p12;
    p2p12.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p12.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer dev01 = p2p01.Install(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer dev12 = p2p12.Install(nodes.Get(1), nodes.Get(2));

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address01;
    address01.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer if01 = address01.Assign(dev01);

    Ipv4AddressHelper address12;
    address12.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer if12 = address12.Assign(dev12);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t port = 9;
    UdpEchoServerHelper server(port);
    ApplicationContainer serverApps = server.Install(nodes.Get(2));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpEchoClientHelper client(if12.GetAddress(1), port);
    client.SetAttribute("MaxPackets", UintegerValue(10));
    client.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = client.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    p2p01.EnablePcapAll("line-topology-udp-p2p01");
    p2p12.EnablePcapAll("line-topology-udp-p2p12");

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}