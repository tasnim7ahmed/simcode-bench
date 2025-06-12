#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TreeTopologySimulation");

int main(int argc, char *argv[])
{
    LogComponentEnable("TreeTopologySimulation", LOG_LEVEL_INFO);
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    // Create 4 nodes: 0=root, 1=child1, 2=child2, 3=leaf (under child2)
    NodeContainer n0n1; // root <-> child1
    NodeContainer n0n2; // root <-> child2
    NodeContainer n2n3; // child2 <-> leaf

    Ptr<Node> n0 = CreateObject<Node>();
    Ptr<Node> n1 = CreateObject<Node>();
    Ptr<Node> n2 = CreateObject<Node>();
    Ptr<Node> n3 = CreateObject<Node>();

    n0n1.Add(n0);
    n0n1.Add(n1);

    n0n2.Add(n0);
    n0n2.Add(n2);

    n2n3.Add(n2);
    n2n3.Add(n3);

    // Install Internet stack
    NodeContainer allNodes;
    allNodes.Add(n0);
    allNodes.Add(n1);
    allNodes.Add(n2);
    allNodes.Add(n3);

    InternetStackHelper stack;
    stack.Install(allNodes);

    // Point-to-point helper
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Install devices and channels
    NetDeviceContainer d0d1 = p2p.Install(n0n1);
    NetDeviceContainer d0d2 = p2p.Install(n0n2);
    NetDeviceContainer d2d3 = p2p.Install(n2n3);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;

    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i0i1 = ipv4.Assign(d0d1);

    ipv4.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer i0i2 = ipv4.Assign(d0d2);

    ipv4.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer i2i3 = ipv4.Assign(d2d3);

    // Enable routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Install a UDP server on node 0 (root)
    uint16_t serverPort = 4000;
    UdpServerHelper server(serverPort);
    ApplicationContainer serverApp = server.Install(n0);
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Install a UDP client on node 3 (leaf)
    UdpClientHelper client(i0i1.GetAddress(0), serverPort);
    client.SetAttribute("MaxPackets", UintegerValue(10));
    client.SetAttribute("Interval", TimeValue(Seconds(0.5)));
    client.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApp = client.Install(n3);
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));
    
    // Enable packet tracing
    p2p.EnablePcapAll("tree-topology");

    Simulator::Stop(Seconds(11.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}