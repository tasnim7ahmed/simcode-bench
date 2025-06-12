#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-dv-routing.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/mobility-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("SplitHorizonDVExample");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("Ipv4DvRouting", LOG_LEVEL_INFO);
    LogComponentEnable("SplitHorizonDVExample", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(3);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("10ms"));

    NetDeviceContainer devicesAB, devicesBC;
    devicesAB = p2p.Install(nodes.Get(0), nodes.Get(1));
    devicesBC = p2p.Install(nodes.Get(1), nodes.Get(2));

    InternetStackHelper stack;

    Ipv4ListRoutingHelper listRH;
    Ipv4DvRoutingHelper dvRouting;

    listRH.Add(dvRouting, 10); // priority 10 for DV routing

    stack.SetRoutingHelper(listRH);
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfacesAB = address.Assign(devicesAB);

    address.SetBase("10.0.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfacesBC = address.Assign(devicesBC);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}