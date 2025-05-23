#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/aodv-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include <cmath> // For std::ceil
#include <string> // For std::to_string

int main(int argc, char *argv[])
{
    // Logging setup (optional, but useful for debugging)
    ns3::LogComponentEnable("UdpClient", ns3::LOG_LEVEL_INFO);
    ns3::LogComponentEnable("UdpServer", ns3::LOG_LEVEL_INFO);
    ns3::LogComponentEnable("AodvRoutingProtocol", ns3::LOG_LEVEL_INFO);

    // Simulation parameters
    double simTime = 20.0;     // seconds
    uint32_t numNodes = 10;
    double nodeSpeed = 5.0;    // m/s
    double nodePause = 2.0;    // seconds
    double areaX = 500.0;      // m
    double areaY = 500.0;      // m
    uint16_t serverPort = 4000;
    uint32_t packetSize = 1024; // bytes
    double sendInterval = 1.0; // seconds

    // Create nodes
    ns3::NodeContainer nodes;
    nodes.Create(numNodes);

    // Set up mobility
    ns3::MobilityHelper mobility;
    mobility.SetPositionAllocator(
        ns3::CreateObject<ns3::RandomRectanglePositionAllocator>(
            ns3::Rectangle(0.0, areaX, 0.0, areaY)
        )
    );
    mobility.SetMobilityModel(
        "ns3::RandomWaypointMobilityModel",
        "Speed", ns3::StringValue("ns3::ConstantRandomVariable[Constant=" + std::to_string(nodeSpeed) + "]"),
        "Pause", ns3::StringValue("ns3::ConstantRandomVariable[Constant=" + std::to_string(nodePause) + "]")
    );
    mobility.Install(nodes);

    // Set up WiFi (802.11b, Ad Hoc mode)
    ns3::WifiHelper wifi;
    wifi.SetStandard(ns3::WIFI_PHY_STANDARD_80211b);

    ns3::YansWifiPhyHelper wifiPhy;
    wifiPhy.SetPcapDataLinkType(ns3::WifiPhyHelper::DLT_IEEE802_11_RADIO);

    ns3::YansWifiChannelHelper wifiChannel;
    wifiChannel.SetPropagationLossModel("ns3::FriisPropagationLossModel");
    wifiChannel.SetPropagationDelayModel("ns3::ConstantSpeedPropagationDelayModel");
    wifiPhy.SetChannel(wifiChannel.Create());

    ns3::NqosWifiMacHelper wifiMac = ns3::NqosWifiMacHelper::CreateAdhocMac();

    ns3::NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    // Install Internet Stack and AODV routing
    ns3::AodvHelper aodv;
    ns3::InternetStackHelper internet;
    internet.SetRoutingHelper(aodv); // Set AODV as the routing protocol
    internet.Install(nodes);

    ns3::Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // Set up UDP Server on node 0
    ns3::UdpServerHelper server(serverPort);
    ns3::ApplicationContainer serverApps = server.Install(nodes.Get(0));
    serverApps.Start(ns3::Seconds(1.0)); // Server starts early
    serverApps.Stop(ns3::Seconds(simTime + 1.0)); // Server stops after simulation end

    // Set up UDP Client on node 1
    ns3::Ptr<ns3::Node> clientNode = nodes.Get(1);
    ns3::Ipv4Address serverAddress = interfaces.GetAddress(0); // Get IP of node 0

    ns3::UdpClientHelper client(serverAddress, serverPort);
    client.SetAttribute("MaxPackets", ns3::UintegerValue(static_cast<uint32_t>(std::ceil(simTime / sendInterval)))); // Send enough packets for duration
    client.SetAttribute("Interval", ns3::TimeValue(ns3::Seconds(sendInterval)));
    client.SetAttribute("PacketSize", ns3::UintegerValue(packetSize));
    ns3::ApplicationContainer clientApps = client.Install(clientNode);
    clientApps.Start(ns3::Seconds(2.0)); // Client starts after server
    clientApps.Stop(ns3::Seconds(simTime + 0.5)); // Client stops slightly after simTime ends (to allow last packet to send)

    // Enable PCAP tracing for all WiFi devices
    wifiPhy.EnablePcapAll("manet-aodv");

    // Run simulation
    ns3::Simulator::Stop(ns3::Seconds(simTime));
    ns3::Simulator::Run();
    ns3::Simulator::Destroy();

    return 0;
}