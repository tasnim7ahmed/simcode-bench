#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

int main(int argc, char *argv[])
{
    ns3::LogComponentEnable("TcpServerApplication", ns3::LOG_LEVEL_INFO);
    ns3::LogComponentEnable("TcpClientApplication", ns3::LOG_LEVEL_INFO);

    ns3::Config::SetDefault("ns3::TcpL4Protocol::SocketType", ns3::StringValue("ns3::TcpNewReno"));

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

    // TCP Server
    uint16_t port = 8080;
    ns3::PacketSinkHelper sink("ns3::TcpSocketFactory", ns3::InetSocketAddress(ns3::Ipv4Address::GetAny(), port));
    ns3::ApplicationContainer serverApps = sink.Install(nodes.Get(0));
    serverApps.Start(ns3::Seconds(1.0));
    serverApps.Stop(ns3::Seconds(10.0));

    // TCP Client
    ns3::Ipv4Address serverAddress = interfaces.GetAddress(0);
    ns3::OnOffHelper client("ns3::TcpSocketFactory", ns3::InetSocketAddress(serverAddress, port));

    uint32_t packetSize = 1024; // 1024 bytes per packet
    uint32_t numPackets = 1000; // 1000 packets
    double interval = 0.01;    // 0.01 seconds interval

    // Calculate DataRate based on packet size and interval
    // Rate = (packetSize * 8 bits) / interval
    ns3::DataRate dataRate = ns3::DataRate((packetSize * 8.0) / interval);

    client.SetAttribute("PacketSize", ns3::UintegerValue(packetSize));
    client.SetAttribute("DataRate", ns3::DataRateValue(dataRate));
    client.SetAttribute("MaxBytes", ns3::UintegerValue(packetSize * numPackets));

    ns3::ApplicationContainer clientApps = client.Install(nodes.Get(1));
    clientApps.Start(ns3::Seconds(2.0));
    clientApps.Stop(ns3::Seconds(10.0)); // Client will stop sending if simulation ends

    ns3::Simulator::Stop(ns3::Seconds(10.0));
    ns3::Simulator::Run();
    ns3::Simulator::Destroy();

    return 0;
}