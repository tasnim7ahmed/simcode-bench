#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);

    NodeContainer nodes;
    nodes.Create(3); // 0: client, 1: router, 2: server

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer ndc1 = p2p.Install(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer ndc2 = p2p.Install(nodes.Get(1), nodes.Get(2));

    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.0.1.0", "255.255.255.0");
    Ipv4InterfaceContainer iface1 = ipv4.Assign(ndc1);

    ipv4.SetBase("10.0.2.0", "255.255.255.0");
    Ipv4InterfaceContainer iface2 = ipv4.Assign(ndc2);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t servPort = 50000;
    Address servAddress(InetSocketAddress(iface2.GetAddress(1), servPort));

    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory",
                                InetSocketAddress(Ipv4Address::GetAny(), servPort));
    ApplicationContainer sinkApp = sinkHelper.Install(nodes.Get(2));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(10.0));

    OnOffHelper clientHelper("ns3::TcpSocketFactory", servAddress);
    clientHelper.SetAttribute("DataRate", StringValue("5Mbps"));
    clientHelper.SetAttribute("PacketSize", UintegerValue(1024));
    clientHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    clientHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));

    ApplicationContainer clientApp = clientHelper.Install(nodes.Get(0));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(9.0));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}