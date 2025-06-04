#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("PacketLossExample");

int main(int argc, char *argv[])
{
    // Set up logging
    LogComponentEnable("PacketLossExample", LOG_LEVEL_INFO);

    // Simulation time
    double simulationTime = 5.0; // seconds

    // Create two nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Create a point-to-point link
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("2Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("5ms"));

    // Install net devices on both nodes
    NetDeviceContainer devices;
    devices = pointToPoint.Install(nodes);

    // Install the internet stack on both nodes
    InternetStackHelper internet;
    internet.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Set up a traffic control helper with a queue to simulate packet loss
    TrafficControlHelper tch;
    tch.SetRootQueueDisc("ns3::RedQueueDisc"); // Use Random Early Detection (RED) queue for packet loss
    tch.Install(devices);

    // Create a UDP server application on Node 1
    uint16_t port = 9; // Port number
    UdpServerHelper server(port);
    ApplicationContainer serverApp = server.Install(nodes.Get(1));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(simulationTime));

    // Create a UDP client application on Node 0
    UdpClientHelper client(interfaces.GetAddress(1), port);
    client.SetAttribute("MaxPackets", UintegerValue(100));
    client.SetAttribute("Interval", TimeValue(Seconds(0.05))); // 50 ms interval between packets
    client.SetAttribute("PacketSize", UintegerValue(1024));    // 1024 bytes per packet

    ApplicationContainer clientApp = client.Install(nodes.Get(0));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(simulationTime));

    // Set up NetAnim for visualization
    AnimationInterface anim("packet_loss_netanim.xml");

    // Set constant positions for NetAnim visualization
    anim.SetConstantPosition(nodes.Get(0), 0, 0); // Node 0 (Sender)
    anim.SetConstantPosition(nodes.Get(1), 5, 0); // Node 1 (Receiver)

    // Enable packet metadata in NetAnim
    anim.EnablePacketMetadata(true);

    // Run the simulation
    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}

