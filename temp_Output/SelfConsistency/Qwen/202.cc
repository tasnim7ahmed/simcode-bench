#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/csma-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/netanim-module.h"
#include "ns3/udp-server-client-helper.h"
#include "ns3/ieee802154-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WSNSimulation");

int main(int argc, char *argv[])
{
    // Enable logging
    LogComponentEnable("WSNSimulation", LOG_LEVEL_INFO);

    // Simulation parameters
    double simulationTime = 60.0; // seconds
    uint32_t packetSize = 128;    // bytes
    uint32_t interval = 5;        // seconds

    // Create nodes
    uint32_t numNodes = 6;
    NodeContainer nodes;
    nodes.Create(numNodes);

    // Create IEEE 802.15.4 channel
    Ieee802154Helper ieee802154 = Ieee802154Helper::Default();
    ieee802154.SetChannelCenterFrequencyMhz(2405); // 2.4 GHz band

    // Install PHY and MAC layers
    NetDeviceContainer devices;
    devices = ieee802154.Install(nodes);

    // Setup mobility
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::CirclePositionAllocator",
                                  "X", DoubleValue(0.0),
                                  "Y", DoubleValue(0.0),
                                  "Z", DoubleValue(0.0),
                                  "Radius", DoubleValue(10.0),
                                  "CircumferencePoints", UintegerValue(numNodes));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(nodes);

    // Assign IPv4 addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // Set sink node (node 0)
    uint32_t sinkNode = 0;

    // Install UDP server on sink
    UdpServerHelper server(9);
    ApplicationContainer serverApps = server.Install(nodes.Get(sinkNode));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(simulationTime));

    // Install UDP clients on other nodes
    UdpClientHelper client(interfaces.GetAddress(sinkNode), 9);
    client.SetAttribute("MaxPackets", UintegerValue((uint32_t)(simulationTime / interval)));
    client.SetAttribute("Interval", TimeValue(Seconds(interval)));
    client.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer clientApps;
    for (uint32_t i = 1; i < numNodes; ++i)
    {
        clientApps.Add(client.Install(nodes.Get(i)));
    }
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(simulationTime));

    // Populate routing tables
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Setup animation
    AnimationInterface anim("wsn-animation.xml");
    anim.EnablePacketMetadata(true);

    // Set node colors in animation
    for (uint32_t i = 0; i < numNodes; ++i)
    {
        if (i == sinkNode)
        {
            anim.UpdateNodeColor(nodes.Get(i), 255, 0, 0); // Red for sink
        }
        else
        {
            anim.UpdateNodeColor(nodes.Get(i), 0, 0, 255); // Blue for sensors
        }
    }

    // Enable pcap tracing
    ieee802154.EnablePcapAll("wsn-packet-trace", false);

    // Schedule simulation
    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    // Output performance metrics
    UdpServer serverApp = DynamicCast<UdpServer>(serverApps.Get(0));
    uint32_t receivedPackets = serverApp->GetReceived();
    uint32_t sentPackets = (numNodes - 1) * ((uint32_t)(simulationTime / interval));
    double packetDeliveryRatio = (double)receivedPackets / (double)sentPackets;
    Time averageDelay = serverApp->GetAverageDelay();

    NS_LOG_UNCOND("Performance Metrics:");
    NS_LOG_UNCOND("Total Sent Packets: " << sentPackets);
    NS_LOG_UNCOND("Total Received Packets: " << receivedPackets);
    NS_LOG_UNCOND("Packet Delivery Ratio: " << packetDeliveryRatio);
    NS_LOG_UNCOND("Average End-to-End Delay: " << averageDelay.As(Time::S));
    NS_LOG_UNCOND("Throughput: " << (receivedPackets * packetSize * 8 / simulationTime) / 1000.0 << " kbps");

    Simulator::Destroy();

    return 0;
}