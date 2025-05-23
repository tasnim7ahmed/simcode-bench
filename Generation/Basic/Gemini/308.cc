#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/bulk-send-helper.h"
#include "ns3/packet-sink-helper.h"

NS_LOG_COMPONENT_DEFINE("WiredPointToPointTcpSimulation");

int main(int argc, char *argv[])
{
    // Set simulation duration
    double simulationTime = 10.0; // seconds

    // Create two nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Create PointToPointHelper and set link attributes
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Install point-to-point devices on nodes
    NetDeviceContainer devices = p2p.Install(nodes);

    // Install internet stack on nodes
    InternetStackHelper internet;
    internet.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // Configure TCP Sink (Receiver) on Node 1
    uint16_t sinkPort = 9;
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory",
                                      InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sinkApps = packetSinkHelper.Install(nodes.Get(1)); // Node 1 is the receiver
    sinkApps.Start(Seconds(1.0));
    sinkApps.Stop(Seconds(simulationTime));

    // Configure BulkSend (Sender) on Node 0
    Ptr<Ipv4> ipv4OfReceiver = nodes.Get(1)->GetObject<Ipv4>();
    Ipv4Address receiverIpAddress = ipv4OfReceiver->GetAddress(1, 0).GetLocal();

    BulkSendHelper bulkSendHelper("ns3::TcpSocketFactory",
                                  InetSocketAddress(receiverIpAddress, sinkPort));
    // Set MaxBytes to 0 for unlimited data
    bulkSendHelper.SetAttribute("MaxBytes", UintegerValue(0));

    ApplicationContainer sourceApps = bulkSendHelper.Install(nodes.Get(0)); // Node 0 is the sender
    sourceApps.Start(Seconds(2.0));
    sourceApps.Stop(Seconds(simulationTime));

    // Enable tracing (optional)
    p2p.EnablePcapAll("wired-point-to-point-tcp");

    // Run the simulation
    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}