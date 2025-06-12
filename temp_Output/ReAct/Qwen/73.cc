#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ThreeRouterSimulation");

int main(int argc, char *argv[]) {
    Config::SetDefault("ns3::OnOffApplication::PacketSize", UintegerValue(1024));
    Config::SetDefault("ns3::OnOffApplication::DataRate", StringValue("1Mbps"));

    NodeContainer nodes;
    nodes.Create(3);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devAB, devBC;
    devAB = p2p.Install(nodes.Get(0), nodes.Get(1));
    devBC = p2p.Install(nodes.Get(1), nodes.Get(2));

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;

    address.SetBase("10.1.1.0", "255.255.255.252"); // /30 subnet for A-B link
    Ipv4InterfaceContainer ifAB = address.Assign(devAB);

    address.SetBase("10.1.2.0", "255.255.255.252"); // /30 subnet for B-C link
    Ipv4InterfaceContainer ifBC = address.Assign(devBC);

    // Assign specific IP addresses
    Ptr<Ipv4> ipv4A = nodes.Get(0)->GetObject<Ipv4>();
    ipv4A->SetIpForward(Ipv4Address("255.255.255.255"), true); // Enable forwarding
    Ipv4InterfaceAddress ifAddrA = Ipv4InterfaceAddress("1.1.1.1", "255.255.255.255");
    ipv4A->AddAddress(0, ifAddrA);

    Ptr<Ipv4> ipv4C = nodes.Get(2)->GetObject<Ipv4>();
    Ipv4InterfaceAddress ifAddrC = Ipv4InterfaceAddress("3.3.3.3", "255.255.255.255");
    ipv4C->AddAddress(0, ifAddrC);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t port = 9;
    OnOffHelper onoff("ns3::UdpSocketFactory", Address(InetSocketAddress(ifAddrC.GetLocal(), port)));
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));

    ApplicationContainer apps = onoff.Install(nodes.Get(0));
    apps.Start(Seconds(1.0));
    apps.Stop(Seconds(10.0));

    PacketSinkHelper sink("ns3::UdpSocketFactory", Address(InetSocketAddress(Ipv4Address::GetAny(), port)));
    apps = sink.Install(nodes.Get(2));
    apps.Start(Seconds(0.0));

    AsciiTraceHelper ascii;
    p2p.EnableAsciiAll(ascii.CreateFileStream("three-router-sim.tr"));
    p2p.EnablePcapAll("three-router-sim");

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}