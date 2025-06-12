#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/ssid.h"
#include "ns3/mobility-module.h"
#include "ns3/ipv4-address-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiAdhocExample");

int main(int argc, char *argv[])
{
    // Enable log components
    LogComponentEnable("WifiAdhocExample", LOG_LEVEL_INFO);

    // Create nodes for the network
    NodeContainer nodes;
    nodes.Create(2); // Two nodes: Sender and Receiver

    // Set up mobility model (place nodes at fixed positions)
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(10.0),
                                  "DeltaY", DoubleValue(10.0),
                                  "GridWidth", UintegerValue(1),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // Set up WiFi helper for wireless communication
    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

    // Set up WiFi PHY (physical layer)
    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    wifiPhy.SetChannel(YansWifiChannelHelper::Default().Create());

    // Set up WiFi MAC (media access control) for ad-hoc mode
    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");

    // Install WiFi devices on nodes
    NetDeviceContainer devices;
    devices = wifi.Install(wifiPhy, wifiMac, nodes);

    // Install the internet stack (TCP/IP stack)
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses to the devices
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // Set up the UDP receiver application on Node 1 (Receiver)
    UdpServerHelper udpServer(9);                                     // UDP port 9
    ApplicationContainer serverApp = udpServer.Install(nodes.Get(1)); // Install on Node 1 (Receiver)
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Set up the UDP sender application on Node 0 (Sender)
    UdpClientHelper udpClient(interfaces.GetAddress(1), 9);      // Send to Node 1
    udpClient.SetAttribute("MaxPackets", UintegerValue(10));     // Number of packets to send
    udpClient.SetAttribute("Interval", TimeValue(Seconds(1.0))); // Interval between packets
    udpClient.SetAttribute("PacketSize", UintegerValue(1024));   // Packet size in bytes

    ApplicationContainer clientApp = udpClient.Install(nodes.Get(0)); // Install on Node 0 (Sender)
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
