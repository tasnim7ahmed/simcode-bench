#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

int main(int argc, char *argv[])
{
    ns3::NodeContainer nodes;
    nodes.Create(2);

    ns3::PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", ns3::StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", ns3::StringValue("5ms"));

    ns3::NetDeviceContainer devices = p2p.Install(nodes);

    ns3::InternetStackHelper stack;
    stack.Install(nodes);

    ns3::Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    uint16_t port = 9;
    ns3::Address serverAddress = ns3::InetSocketAddress(ns3::Ipv4Address::GetAny(), port);
    ns3::PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", serverAddress);
    ns3::ApplicationContainer serverApp = sinkHelper.Install(nodes.Get(1));
    serverApp.Start(ns3::Seconds(0.0));
    serverApp.Stop(ns3::Seconds(10.0));

    ns3::Ipv4Address serverNodeIp = interfaces.GetAddress(1);
    ns3::Address clientAddress = ns3::InetSocketAddress(serverNodeIp, port);
    ns3::BulkSendHelper clientHelper("ns3::TcpSocketFactory", clientAddress);
    clientHelper.SetAttribute("MaxBytes", ns3::UintegerValue(0));

    ns3::ApplicationContainer clientApp = clientHelper.Install(nodes.Get(0));
    clientApp.Start(ns3::Seconds(1.0));
    clientApp.Stop(ns3::Seconds(10.0));

    ns3::Simulator::Stop(ns3::Seconds(10.0));

    ns3::Simulator::Run();

    ns3::Simulator::Destroy();

    return 0;
}