#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("BranchTopology");

int main(int argc, char* argv[]) {
    LogComponent::SetAttribute("UdpClient", "Interval", StringValue("0.1s"));

    NodeContainer nodes;
    nodes.Create(4);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices1 = pointToPoint.Install(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer devices2 = pointToPoint.Install(nodes.Get(0), nodes.Get(2));
    NetDeviceContainer devices3 = pointToPoint.Install(nodes.Get(0), nodes.Get(3));

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces1 = address.Assign(devices1);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces2 = address.Assign(devices2);

    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces3 = address.Assign(devices3);

    UdpClientHelper client1(interfaces1.GetAddress(1), 9); // Node 1 to Node 0
    client1.SetAttribute("MaxPackets", UintegerValue(1000));
    client1.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps1 = client1.Install(nodes.Get(1));
    clientApps1.Start(Seconds(1.0));
    clientApps1.Stop(Seconds(10.0));

    UdpClientHelper client2(interfaces2.GetAddress(1), 9); // Node 2 to Node 0
    client2.SetAttribute("MaxPackets", UintegerValue(1000));
    client2.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps2 = client2.Install(nodes.Get(2));
    clientApps2.Start(Seconds(2.0));
    clientApps2.Stop(Seconds(10.0));

    UdpClientHelper client3(interfaces3.GetAddress(1), 9); // Node 3 to Node 0
    client3.SetAttribute("MaxPackets", UintegerValue(1000));
    client3.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps3 = client3.Install(nodes.Get(3));
    clientApps3.Start(Seconds(3.0));
    clientApps3.Stop(Seconds(10.0));

    UdpServerHelper server(9);
    ApplicationContainer serverApps = server.Install(nodes.Get(0));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(10.0));

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    pointToPoint.EnablePcapAll("branch", false);

    Simulator::Run(Seconds(10.0));
    Simulator::Destroy();

    return 0;
}