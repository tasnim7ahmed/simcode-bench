#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/netanim-module.h"
#include "ns3/traffic-control-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(7);

    // Create point-to-point links
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices1_2 = pointToPoint.Install(nodes.Get(1), nodes.Get(2));
    NetDeviceContainer devices2_3 = pointToPoint.Install(nodes.Get(2), nodes.Get(3));
    NetDeviceContainer devices3_6 = pointToPoint.Install(nodes.Get(3), nodes.Get(6));
    NetDeviceContainer devices1_4 = pointToPoint.Install(nodes.Get(1), nodes.Get(4));
    NetDeviceContainer devices4_5 = pointToPoint.Install(nodes.Get(4), nodes.Get(5));
    NetDeviceContainer devices5_6 = pointToPoint.Install(nodes.Get(5), nodes.Get(6));

    // Create CSMA/CD link
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("10Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(MicroSeconds(10)));
    NetDeviceContainer csmaDevices = csma.Install(NodeContainer(nodes.Get(2), nodes.Get(4), nodes.Get(5)));

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces1_2 = address.Assign(devices1_2);
    address.NewNetwork();
    Ipv4InterfaceContainer interfaces2_3 = address.Assign(devices2_3);
    address.NewNetwork();
    Ipv4InterfaceContainer interfaces3_6 = address.Assign(devices3_6);
    address.NewNetwork();
    Ipv4InterfaceContainer interfaces1_4 = address.Assign(devices1_4);
    address.NewNetwork();
    Ipv4InterfaceContainer interfaces4_5 = address.Assign(devices4_5);
    address.NewNetwork();
    Ipv4InterfaceContainer interfaces5_6 = address.Assign(devices5_6);
    address.NewNetwork();
    Ipv4InterfaceContainer csmaInterfaces = address.Assign(csmaDevices);
    address.NewNetwork();

    // Enable global routing with automatic interface event response
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();
    Ipv4GlobalRoutingHelper::EnablePcapAll("global-routing");

    // Create CBR traffic flow from node 1 to node 6
    uint16_t port = 9;
    OnOffHelper onoff("ns3::UdpSocketFactory",
                     Address(InetSocketAddress(interfaces3_6.GetAddress(1), port)));
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff.SetAttribute("PacketSize", UintegerValue(1024));
    onoff.SetAttribute("DataRate", StringValue("1Mbps"));

    ApplicationContainer app1 = onoff.Install(nodes.Get(1));
    app1.Start(Seconds(1.0));
    app1.Stop(Seconds(20.0));

    OnOffHelper onoff2("ns3::UdpSocketFactory",
                      Address(InetSocketAddress(interfaces5_6.GetAddress(1), port)));
    onoff2.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff2.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff2.SetAttribute("PacketSize", UintegerValue(1024));
    onoff2.SetAttribute("DataRate", StringValue("1Mbps"));

    ApplicationContainer app2 = onoff2.Install(nodes.Get(1));
    app2.Start(Seconds(11.0));
    app2.Stop(Seconds(20.0));

    // Simulate interface down and up events
    Simulator::Schedule(Seconds(5.0), &Ipv4Interface::SetDown, interfaces2_3.Get(0));
    Simulator::Schedule(Seconds(8.0), &Ipv4Interface::SetUp, interfaces2_3.Get(0));
    Simulator::Schedule(Seconds(15.0), &Ipv4Interface::SetDown, interfaces4_5.Get(0));
    Simulator::Schedule(Seconds(18.0), &Ipv4Interface::SetUp, interfaces4_5.Get(0));

    // Tracing
    pointToPoint.EnablePcapAll("global-routing");
    csma.EnablePcapAll("global-routing", false);

    // Enable queue traces
    TrafficControlHelper tch;
    tch.EnableQueueDiscLogging();

    // Run the simulation
    Simulator::Stop(Seconds(20.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}