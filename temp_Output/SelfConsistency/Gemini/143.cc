#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/log.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TreeTopology");

int main() {
    LogComponent::SetAttribute("UdpClient", "MaxSize", UintegerValue(1024));
    LogComponent::SetAttribute("UdpClient", "Interval", TimeValue(Seconds(0.1)));

    // Create nodes
    NodeContainer nodes;
    nodes.Create(4);

    // Create point-to-point links
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    // Connect nodes in a tree topology
    NetDeviceContainer devices01 = pointToPoint.Install(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer devices02 = pointToPoint.Install(nodes.Get(0), nodes.Get(2));
    NetDeviceContainer devices13 = pointToPoint.Install(nodes.Get(1), nodes.Get(3));

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces01 = address.Assign(devices01);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces02 = address.Assign(devices02);

    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces13 = address.Assign(devices13);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Create UDP client application at node 3 and send packets to node 0
    UdpClientHelper client(interfaces01.GetAddress(0), 9);
    client.SetAttribute("MaxPackets", UintegerValue(100));
    client.SetAttribute("StartTime", TimeValue(Seconds(1.0)));
    ApplicationContainer clientApps = client.Install(nodes.Get(3));

    // Create UDP server application at node 0
    UdpServerHelper server(9);
    ApplicationContainer serverApps = server.Install(nodes.Get(0));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(10.0));

    // Enable packet tracing
    pointToPoint.EnablePcapAll("tree_topology");

    // Run the simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}