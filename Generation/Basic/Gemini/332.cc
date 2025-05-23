#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"

int main(int argc, char *argv[])
{
    // Create two nodes for the ad-hoc network
    ns3::NodeContainer nodes;
    nodes.Create(2);

    // Configure mobility: place nodes close to each other
    ns3::MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", ns3::DoubleValue(0.0),
                                  "MinY", ns3::DoubleValue(0.0),
                                  "DeltaX", ns3::DoubleValue(5.0), // Nodes at (0,0,0) and (5,0,0)
                                  "DeltaY", ns3::DoubleValue(0.0),
                                  "GridWidth", ns3::UintegerValue(2),
                                  "LayoutType", ns3::StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // Set up WiFi properties for 802.11b ad-hoc
    ns3::WifiHelper wifi;
    wifi.SetStandard(ns3::WIFI_STANDARD_80211b); // Use 802.11b standard

    // Set up the WiFi physical layer
    ns3::YansWifiPhyHelper wifiPhy;
    wifiPhy.SetPcapDataLinkType(ns3::WifiPhyHelper::DLT_IEEE802_11_RADIO);

    // Set up the WiFi channel with propagation loss and delay models
    ns3::YansWifiChannelHelper wifiChannel;
    wifiChannel.SetPropagationLossModel("ns3::FriisPropagationLossModel");
    wifiChannel.SetPropagationDelayModel("ns3::ConstantSpeedPropagationDelayModel");
    wifiPhy.SetChannel(wifiChannel.Create());

    // Set up the WiFi MAC layer for Ad-hoc mode
    ns3::WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");

    // Install WiFi devices on nodes
    ns3::NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    // Install the Internet stack on nodes
    ns3::InternetStackHelper internet;
    internet.Install(nodes);

    // Assign IP addresses to the devices
    ns3::Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // Configure UDP Receiver (Node 1)
    uint16_t port = 9; // Port for UDP communication
    ns3::UdpServerHelper server(port);
    ns3::ApplicationContainer serverApps = server.Install(nodes.Get(1)); // Node 1 is receiver
    serverApps.Start(ns3::Seconds(1.0));  // Receiver starts at 1 second
    serverApps.Stop(ns3::Seconds(12.0)); // Receiver stops at 12 seconds to ensure reception of all packets

    // Configure UDP Sender (Node 0)
    ns3::Ipv4Address sinkAddress = interfaces.GetAddress(1); // IP address of Node 1
    ns3::UdpClientHelper client(sinkAddress, port);
    client.SetAttribute("Interval", ns3::TimeValue(ns3::Seconds(1.0)));  // Send a packet every 1 second
    client.SetAttribute("PacketSize", ns3::UintegerValue(1024));        // Packet size 1024 bytes
    client.SetAttribute("MaxPackets", ns3::UintegerValue(10));          // Total of 10 packets

    ns3::ApplicationContainer clientApps = client.Install(nodes.Get(0)); // Node 0 is sender
    clientApps.Start(ns3::Seconds(2.0)); // Sender starts at 2 seconds

    // To send 10 packets with 1s interval starting at 2s, the last packet is scheduled at 2 + (10-1)*1 = 11s.
    // The application should stop slightly after 11s to ensure the last send event is processed.
    clientApps.Stop(ns3::Seconds(11.1)); // Sender stops at 11.1 seconds

    // Optional: Enable PCAP tracing for network devices
    // wifiPhy.EnablePcap("wifi-ad-hoc", devices);

    // Run the simulation
    ns3::Simulator::Run();
    ns3::Simulator::Destroy();

    return 0;
}