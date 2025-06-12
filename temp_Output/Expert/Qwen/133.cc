#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/dv-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("SplitHorizonTest");

int main(int argc, char *argv[]) {
    Config::SetDefault("ns3::DvRoutingProtocol::SplitHorizon", BooleanValue(true));

    NodeContainer nodes;
    nodes.Create(3);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devicesAB, devicesBC;
    devicesAB = p2p.Install(nodes.Get(0), nodes.Get(1));
    devicesBC = p2p.Install(nodes.Get(1), nodes.Get(2));

    InternetStackHelper stack;
    DvRoutingHelper routingHelper;
    stack.SetRoutingHelper(routingHelper);
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfacesAB = address.Assign(devicesAB);
    address.NewNetwork();
    Ipv4InterfaceContainer interfacesBC = address.Assign(devicesBC);

    uint32_t port = 9;

    OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(interfacesBC.GetAddress(1), port));
    onoff.SetConstantDataRate(DataRate("1kbps"));
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));

    ApplicationContainer clientApp = onoff.Install(nodes.Get(0));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(10.0));

    PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer serverApp = sink.Install(nodes.Get(2));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(11.0));

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Simulator::Stop(Seconds(11.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}