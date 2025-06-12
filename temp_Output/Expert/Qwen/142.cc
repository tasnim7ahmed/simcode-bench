#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/log.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("StarTopologySimulation");

int main(int argc, char *argv[]) {
    // Enable logging
    LogComponentEnable("StarTopologySimulation", LOG_LEVEL_INFO);

    // Create nodes for star topology
    NodeContainer centralNode;
    centralNode.Create(1);

    NodeContainer peripheralNodes;
    peripheralNodes.Create(2);

    NodeContainer node1 = NodeContainer(centralNode, peripheralNodes.Get(0));
    NodeContainer node2 = NodeContainer(centralNode, peripheralNodes.Get(1));

    // Setup point-to-point links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("1ms"));

    NetDeviceContainer devices1 = p2p.Install(node1);
    NetDeviceContainer devices2 = p2p.Install(node2);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(centralNode);
    stack.Install(peripheralNodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces1 = address.Assign(devices1);

    address.NewNetwork();
    Ipv4InterfaceContainer interfaces2 = address.Assign(devices2);

    // Set up echo server on one peripheral node (node 0 of peripheralNodes)
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(peripheralNodes.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Set up echo client on the other peripheral node (node 1 of peripheralNodes)
    UdpEchoClientHelper echoClient(interfaces1.GetAddress(1), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = echoClient.Install(peripheralNodes.Get(1));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Enable logging of packet transmissions
    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("star-topology-simulation.tr");
    p2p.EnableAsciiAll(stream);

    // Schedule simulation
    Simulator::Stop(Seconds(11.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}