#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("StarTopologySimulation");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("StarTopologySimulation", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer centralNode;
    centralNode.Create(1);

    NodeContainer peripheralNodes;
    peripheralNodes.Create(2);

    // Connect peripheral nodes to the central node
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("1ms"));

    NetDeviceContainer link1 = p2p.Install(centralNode.Get(0), peripheralNodes.Get(0));
    NetDeviceContainer link2 = p2p.Install(centralNode.Get(0), peripheralNodes.Get(1));

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(centralNode);
    stack.Install(peripheralNodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces1 = address.Assign(link1);
    address.NewNetwork();
    Ipv4InterfaceContainer interfaces2 = address.Assign(link2);

    // Enable global routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Set up packet sink and UDP echo client on peripheral nodes
    uint16_t port = 9;

    // Server on peripheral node 1
    PacketSinkHelper sinkHelper("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApp = sinkHelper.Install(peripheralNodes.Get(1));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(10.0));

    // Client on peripheral node 0
    UdpEchoClientHelper client(peripheralNodes.Get(1)->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal(), port);
    client.SetAttribute("MaxPackets", UintegerValue(5));
    client.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = client.Install(peripheralNodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Enable logging of packet transmissions
    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("star-topology-simulation.tr");
    p2p.EnableAsciiAll(stream);

    // Enable pcap tracing for detailed packet inspection
    p2p.EnablePcapAll("star-topology-simulation");

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}