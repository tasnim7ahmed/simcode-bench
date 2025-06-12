#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/aodv-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Set simulation parameters
    double duration = 30.0;
    uint32_t numNodes = 10;
    std::string phyMode("DsssRate1Mbps");
    uint32_t packetSize = 1024;
    double dataRate = 1.0; // Mbps

    // Enable AODV logging
    LogComponentEnable("AodvRoutingProtocol", LOG_LEVEL_ALL);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(numNodes);

    // Setup Mobility - Random movement within 500x500m area
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                  "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=500.0]"),
                                  "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=500.0]"));
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(0, 500, 0, 500)),
                              "Distance", DoubleValue(50));
    mobility.Install(nodes);

    // Setup WiFi
    WifiMacHelper wifiMac;
    WifiPhyHelper wifiPhy;
    wifiPhy.Set("TxPowerStart", DoubleValue(16.0206));
    wifiPhy.Set("TxPowerEnd", DoubleValue(16.0206));
    wifiPhy.Set("TxGain", DoubleValue(0));
    wifiPhy.Set("RxGain", DoubleValue(0));
    wifiPhy.Set("ChannelWidth", UintegerValue(20));
    wifiPhy.Set("Frequency", UintegerValue(2412));
    wifiPhy.Set("CcaEdThreshold", DoubleValue(-69.999991));

    wifiMac.SetType("ns3::AdhocWifiMac");
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    NetDeviceContainer devices = wifiMac.Install(wifiPhy, nodes);

    // Install Internet stack with AODV
    AodvHelper aodv;
    InternetStackHelper internet;
    internet.SetRoutingHelper(aodv);
    internet.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // Setup UDP traffic generator
    ApplicationContainer clientApps;
    ApplicationContainer serverApps;

    for (uint32_t i = 0; i < numNodes; ++i) {
        // Set up UDP echo server on each node
        UdpEchoServerHelper server(9);
        serverApps.Add(server.Install(nodes.Get(i)));
    }

    // Start servers at time 0
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(duration));

    // Generate random traffic between nodes
    Ptr<UniformRandomVariable> rand = CreateObject<UniformRandomVariable>();
    for (uint32_t i = 0; i < numNodes; ++i) {
        uint32_t destNode;
        do {
            destNode = rand->GetInteger(0, numNodes - 1);
        } while (destNode == i);  // Avoid self-communication

        double startTime = rand->GetValue(1.0, duration - 1.0);

        UdpEchoClientHelper client(interfaces.GetAddress(destNode), 9);
        client.SetAttribute("MaxPackets", UintegerValue(10));
        client.SetAttribute("Interval", TimeValue(Seconds(1.0)));
        client.SetAttribute("PacketSize", UintegerValue(packetSize));

        ApplicationContainer app = client.Install(nodes.Get(i));
        app.Start(Seconds(startTime));
        app.Stop(Seconds(duration));
        clientApps.Add(app);
    }

    // Enable pcap tracing
    wifiPhy.EnablePcapAll("wireless-aodv");

    // Enable ASCII tracing
    AsciiTraceHelper ascii;
    wifiPhy.EnableAsciiAll(ascii.CreateFileStream("wireless-aodv.tr"));

    // Run simulation
    Simulator::Stop(Seconds(duration));
    Simulator::Run();

    // Calculate statistics
    uint32_t totalSent = 0, totalReceived = 0;
    Time totalDelay = Seconds(0);

    for (auto it = clientApps.Begin(); it != clientApps.End(); ++it) {
        UdpEchoClient* client = dynamic_cast<UdpEchoClient*>(*it);
        if (client) {
            totalSent += client->GetTotalTransmittedPackets();
        }
    }

    for (auto it = serverApps.Begin(); it != serverApps.End(); ++it) {
        UdpEchoServer* server = dynamic_cast<UdpEchoServer*>(*it);
        if (server) {
            totalReceived += server->GetReceivedPacketCount();
            totalDelay += server->GetAverageE2EDelay();
        }
    }

    if (totalSent > 0) {
        double pdr = (double)totalReceived / totalSent;
        double avgDelay = (totalReceived > 0) ? totalDelay.GetSeconds() / totalReceived : 0;
        NS_LOG_UNCOND("Packet Delivery Ratio: " << pdr);
        NS_LOG_UNCOND("Average End-to-End Delay: " << avgDelay << " s");
    }

    Simulator::Destroy();
    return 0;
}