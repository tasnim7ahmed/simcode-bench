#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/udp-client-server-helper.h"

// NS_LOG_COMPONENT_DEFINE ("WifiTwoNodesUdp"); // Not needed for the final output as per instructions.

int main (int argc, char *argv[])
{
    // Enable logging for UDP client and server applications
    // LogComponentEnable ("UdpClientApplication", LOG_LEVEL_INFO);
    // LogComponentEnable ("UdpServerApplication", LOG_LEVEL_INFO);

    // 1. Create two nodes
    NodeContainer nodes;
    nodes.Create (2);

    // 2. Install mobility models
    MobilityHelper mobility;
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.Install (nodes.Get (0)); // Node A
    mobility.Install (nodes.Get (1)); // Node B

    // Set fixed positions for nodes
    Ptr<ConstantPositionMobilityModel> posA = nodes.Get(0)->GetObject<ConstantPositionMobilityModel>();
    posA->SetPosition (Vector (0.0, 0.0, 0.0));

    Ptr<ConstantPositionMobilityModel> posB = nodes.Get(1)->GetObject<ConstantPositionMobilityModel>();
    posB->SetPosition (Vector (10.0, 0.0, 0.0)); // Place B 10 meters away from A

    // 3. Install the Internet stack on the nodes
    InternetStackHelper internet;
    internet.Install (nodes);

    // 4. Configure Wi-Fi communication
    WifiHelper wifi;
    wifi.SetStandard (WIFI_PHY_STANDARD_80211n_5GHZ); // Using 802.11n 5GHz

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
    YansWifiPhyHelper phy;
    phy.SetChannel (channel.Create ());

    // Adhoc mode for direct communication between two nodes
    WifiMacHelper mac;
    mac.SetType ("ns3::AdhocWifiMac");

    // Set a constant rate for Wi-Fi communication
    wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                  "DataMode", StringValue ("HtMcs0"),
                                  "ControlMode", StringValue ("HtMcs0"));

    NetDeviceContainer devices = wifi.Install (phy, mac, nodes);

    // 5. Assign IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase ("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign (devices);

    // 6. Setup UDP applications
    uint16_t port = 9; // Echo port

    // UDP Server on Node B (index 1)
    UdpServerHelper server (port);
    ApplicationContainer serverApps = server.Install (nodes.Get (1));
    serverApps.Start (Seconds (1.0));
    serverApps.Stop (Seconds (10.0));

    // UDP Client on Node A (index 0)
    // Destination is Node B's IP address
    UdpClientHelper client (interfaces.GetAddress (1), port);
    client.SetAttribute ("MaxPackets", UintegerValue (100)); // 10s / 100ms = 100 packets
    client.SetAttribute ("Interval", TimeValue (MilliSeconds (100))); // 100ms interval
    client.SetAttribute ("PacketSize", UintegerValue (128)); // 128-byte packets

    ApplicationContainer clientApps = client.Install (nodes.Get (0));
    clientApps.Start (Seconds (1.0));
    clientApps.Stop (Seconds (10.0));

    // 7. Enable PCAP tracing for Wi-Fi
    phy.EnablePcap ("wifi-two-nodes", devices);

    // 8. Run the simulation
    Simulator::Stop (Seconds (10.0));
    Simulator::Run ();
    Simulator::Destroy ();

    return 0;
}