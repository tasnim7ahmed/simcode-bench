#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("P2PTcpSimulation");

int main(int argc, char *argv[]) {
    // Enable logging for BulkSendApplication and PacketSink
    LogComponentEnable("BulkSendApplication", LOG_LEVEL_INFO);
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);

    // Create two nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Setup point-to-point link with 5 Mbps data rate and 2 ms delay
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    // Install network devices on the nodes
    NetDeviceContainer devices = pointToPoint.Install(nodes);

    // Assign IP addresses using the 10.1.1.0/24 subnet
    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Setup TCP connection from Node 0 to Node 1
    uint16_t sinkPort = 8080; // Well-known port for receiving

    // Install PacketSink on Node 1
    PacketSinkHelper sink("ns3::TcpSocketFactory",
                          InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sinkApp = sink.Install(nodes.Get(1));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(10.0));

    // Install BulkSend application on Node 0
    BulkSendHelper sender("ns3::TcpSocketFactory",
                          InetSocketAddress(interfaces.GetAddress(1), sinkPort));
    sender.SetAttribute("MaxBytes", UintegerValue(0)); // Unlimited sending
    ApplicationContainer senderApp = sender.Install(nodes.Get(0));
    senderApp.Start(Seconds(1.0));
    senderApp.Stop(Seconds(10.0));

    // Set up flow monitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    // Output results
    monitor->CheckForLostPackets();
    std::cout << "\nFlow Monitor Results:\n";
    monitor->SerializeToXmlFile("p2p-tcp-flow.xml", false, false);

    return 0;
}