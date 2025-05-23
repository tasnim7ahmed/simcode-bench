#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

int main(int argc, char *argv[])
{
    ns3::LogComponentEnable("UdpServerApplication", ns3::LOG_LEVEL_INFO);
    ns3::LogComponentEnable("UdpClientApplication", ns3::LOG_LEVEL_INFO);

    ns3::NodeContainer nodes;
    nodes.Create(2);

    ns3::PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", ns3::StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", ns3::StringValue("2ms"));

    ns3::NetDeviceContainer devices = p2p.Install(nodes);

    ns3::InternetStackHelper stack;
    stack.Install(nodes);

    ns3::Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer interfaces = address.Assign(devices);

    ns3::Ipv4Address serverAddress = interfaces.GetAddress(1);

    uint16_t port = 9;
    ns3::UdpServerHelper serverApp(port);
    ns3::ApplicationContainer serverApps = serverApp.Install(nodes.Get(1));
    serverApps.Start(ns3::Seconds(0.0));
    serverApps.Stop(ns3::Seconds(10.0));

    ns3::UdpClientHelper clientApp(serverAddress, port);
    clientApp.SetAttribute("MaxPackets", ns3::UintegerValue(1));
    clientApp.SetAttribute("Interval", ns3::TimeValue(ns3::Seconds(1.0)));
    clientApp.SetAttribute("PacketSize", ns3::UintegerValue(100));

    ns3::ApplicationContainer clientApps = clientApp.Install(nodes.Get(0));
    clientApps.Start(ns3::Seconds(1.0));
    clientApps.Stop(ns3::Seconds(10.0));

    ns3::Simulator::Stop(ns3::Seconds(10.0));
    ns3::Simulator::Run();
    ns3::Simulator::Destroy();

    return 0;
}