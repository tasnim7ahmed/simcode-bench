#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"

int main(int argc, char *argv[])
{
    // 1. Create nodes: one AP, two stations
    NodeContainer nodes;
    nodes.Create(3); // apNode, staNode1 (client), staNode2 (server)

    Ptr<Node> apNode = nodes.Get(0);
    Ptr<Node> staNode1 = nodes.Get(1); // UDP Client
    Ptr<Node> staNode2 = nodes.Get(2); // UDP Server

    // 2. Configure Wi-Fi Channel and PHY
    WifiHelper wifi;
    wifi.SetStandard(ns3::WIFI_STANDARD_80211n); // Use 802.11n standard

    YansWifiChannelHelper channel;
    channel.SetPropagationLoss("ns3::LogDistancePropagationLossModel");
    channel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    Ptr<YansWifiChannel> wifiChannel = channel.Create();

    YansWifiPhyHelper phy;
    phy.SetChannel(wifiChannel);
    // Set transmit power (optional, but good for range)
    phy.Set("TxPowerStart", DoubleValue(20)); // dBm
    phy.Set("TxPowerEnd", DoubleValue(20));   // dBm

    // 3. Configure Wi-Fi MAC layer (infrastructure mode)
    NqosWifiMacHelper mac = NqosWifiMacHelper::Default();
    Ssid ssid = Ssid("ns3-wifi-network"); // Define a common SSID

    // Install AP Wi-Fi device
    NetDeviceContainer apDevices;
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid),
                "BeaconInterval", TimeValue(MicroSeconds(102400)));
    apDevices = wifi.Install(phy, mac, apNode);

    // Install STA Wi-Fi devices
    NetDeviceContainer staDevices;
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid));
    staDevices.Add(wifi.Install(phy, mac, staNode1));
    staDevices.Add(wifi.Install(phy, mac, staNode2));

    // 4. Set up Mobility Model (static positions)
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0), // Spacing nodes by 5 meters
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(3), // One row of 3 nodes
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes); // Install mobility for all nodes

    // 5. Install Internet Stack (IPv4)
    InternetStackHelper stack;
    stack.Install(nodes);

    // 6. Assign IPv4 Addresses
    Ipv4AddressHelper address;
    address.SetBase("10.0.0.0", "255.255.255.0"); // Network 10.0.0.0/24

    Ipv4InterfaceContainer apInterfaces = address.Assign(apDevices);
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);

    // Get IP addresses of the stations for application setup
    Ipv4Address sta1IpAddress = staInterfaces.GetAddress(0); // Client: 10.0.0.2
    Ipv4Address sta2IpAddress = staInterfaces.GetAddress(1); // Server: 10.0.0.3

    // 7. Setup UDP Echo Server and Client
    uint16_t port = 9; // Standard UDP Echo port

    // Install UDP Echo Server on staNode2
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(staNode2);
    serverApps.Start(Seconds(1.0)); // Server starts at 1 second
    serverApps.Stop(Seconds(20.0)); // Server stops at 20 seconds

    // Install UDP Echo Client on staNode1, targeting staNode2
    UdpEchoClientHelper echoClient(sta2IpAddress, port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(1));    // Client sends one packet
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0))); // Interval (not critical for 1 packet)
    echoClient.SetAttribute("PacketSize", UintegerValue(512));  // 512-byte message
    ApplicationContainer clientApps = echoClient.Install(staNode1);
    clientApps.Start(Seconds(2.0)); // Client starts at 2 seconds
    clientApps.Stop(Seconds(20.0)); // Client stops at 20 seconds

    // 8. Enable PCAP tracing
    // Capture traffic on all Wi-Fi PHY devices
    phy.EnablePcapAll("wifi-udp-echo");

    // 9. Run the simulation
    Simulator::Stop(Seconds(20.0)); // Simulation runs for 20 seconds
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}