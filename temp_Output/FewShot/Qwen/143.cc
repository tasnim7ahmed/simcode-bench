#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/on-off-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging for packet transmission
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);

    // Create 4 nodes: root (0), children (1 and 2), leaf (3)
    NodeContainer root;
    root.Create(1);

    NodeContainer child1, child2;
    child1.Create(1);
    child2.Create(1);

    NodeContainer leaf;
    leaf.Create(1);

    // Connect root to child1 and child2
    NodeContainer p2pRootChild1(root, child1);
    NodeContainer p2pRootChild2(root, child2);

    // Connect child1 to leaf
    NodeContainer p2pChild1Leaf(child1, leaf);

    // Configure point-to-point links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Install P2P links
    NetDeviceContainer devRootChild1 = p2p.Install(p2pRootChild1);
    NetDeviceContainer devRootChild2 = p2p.Install(p2pRootChild2);
    NetDeviceContainer devChild1Leaf = p2p.Install(p2pChild1Leaf);

    // Install Internet stack on all nodes
    InternetStackHelper stack;
    stack.InstallAll();

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer ifRootChild1 = address.Assign(devRootChild1);
    address.NewNetwork();
    Ipv4InterfaceContainer ifRootChild2 = address.Assign(devRootChild2);
    address.NewNetwork();
    Ipv4InterfaceContainer ifChild1Leaf = address.Assign(devChild1Leaf);

    // Set up a PacketSink on the root node (node 0) to receive packets
    uint16_t sinkPort = 8080;
    PacketSinkHelper sinkHelper("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sinkApp = sinkHelper.Install(root.Get(0));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(10.0));

    // Set up OnOff application on the leaf node to send packets to root node
    OnOffHelper onOffHelper("ns3::UdpSocketFactory", InetSocketAddress(ifRootChild1.GetAddress(0), sinkPort));
    onOffHelper.SetAttribute("DataRate", DataRateValue(DataRate("1Mbps")));
    onOffHelper.SetAttribute("PacketSize", UintegerValue(1024));
    onOffHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onOffHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));

    ApplicationContainer clientApp = onOffHelper.Install(leaf.Get(0));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(10.0));

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}