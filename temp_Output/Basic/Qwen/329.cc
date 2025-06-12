#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/tcp-header.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpThreeNodes");

int main(int argc, char *argv[]) {
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpSocketFactory::GetTypeId()));

    NodeContainer nodes;
    nodes.Create(3);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", DataRateValue(DataRate("1Mbps")));
    p2p.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

    NetDeviceContainer devices0, devices1;
    devices0 = p2p.Install(nodes.Get(0), nodes.Get(2));
    devices1 = p2p.Install(nodes.Get(1), nodes.Get(2));

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces0 = address.Assign(devices0);
    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces1 = address.Assign(devices1);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t port = 9;

    // Server application on Node 2
    PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer serverApp = sink.Install(nodes.Get(2));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(10.0));

    // Client applications
    OnOffHelper client0("ns3::TcpSocketFactory", InetSocketAddress(interfaces0.GetAddress(1), port));
    client0.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    client0.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    client0.SetAttribute("DataRate", DataRateValue(DataRate("500kbps")));
    client0.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApp0 = client0.Install(nodes.Get(0));
    clientApp0.Start(Seconds(2.0));
    clientApp0.Stop(Seconds(10.0));

    OnOffHelper client1("ns3::TcpSocketFactory", InetSocketAddress(interfaces1.GetAddress(1), port));
    client1.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    client1.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    client1.SetAttribute("DataRate", DataRateValue(DataRate("500kbps")));
    client1.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApp1 = client1.Install(nodes.Get(1));
    clientApp1.Start(Seconds(3.0));
    clientApp1.Stop(Seconds(10.0));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}