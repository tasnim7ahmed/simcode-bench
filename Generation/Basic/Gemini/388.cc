#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Set time resolution for the simulation
    Time::SetResolution(NanoSeconds(1));

    // Enable logging for UDP echo applications for debugging (optional)
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create two nodes for the Wi-Fi network
    NodeContainer nodes;
    nodes.Create(2);

    // Set up static positions for the nodes
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // Set specific positions for Node 0 and Node 1
    Ptr<ConstantPositionMobilityModel> node0Mobility = nodes.Get(0)->GetObject<ConstantPositionMobilityModel>();
    node0Mobility->SetPosition(Vector(0.0, 0.0, 0.0));

    Ptr<ConstantPositionMobilityModel> node1Mobility = nodes.Get(1)->GetObject<ConstantPositionMobilityModel>();
    node1Mobility->SetPosition(Vector(5.0, 5.0, 0.0));

    // Install the Internet stack on the nodes
    InternetStackHelper internet;
    internet.Install(nodes);

    // Configure Wi-Fi devices
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n_5GHZ); // Use a modern Wi-Fi standard
    wifi.SetRemoteStationManager("ns3::AarfWifiManager"); // Set AARF Wi-Fi manager

    YansWifiPhyHelper wifiPhy;
    // Set data link type for PCAP tracing if desired
    // wifiPhy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);

    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac"); // Configure Wi-Fi for ad-hoc mode

    // Install Wi-Fi devices on the nodes
    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    // Assign IP addresses to the Wi-Fi devices
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // Configure UDP applications

    // Server (Node 1) setup
    uint16_t port = 9; // Server listening port
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(1));
    serverApps.Start(Seconds(0.0)); // Server starts at the beginning of simulation
    serverApps.Stop(Seconds(10.0)); // Server stops at 10 seconds

    // Client (Node 0) setup
    // Destination is Node 1's IP address (interfaces.GetAddress(1) for Node 1)
    UdpEchoClientHelper echoClient(interfaces.GetAddress(1), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(4294967295U)); // Send a very large number of packets
    echoClient.SetAttribute("Interval", TimeValue(MilliSeconds(1))); // Send a packet every 1 millisecond
    echoClient.SetAttribute("PacketSize", UintegerValue(1024)); // Default packet size

    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(1.0)); // Client starts slightly after the server
    clientApps.Stop(Seconds(10.0)); // Client stops at 10 seconds

    // Set the overall simulation stop time
    Simulator::Stop(Seconds(10.0));

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}