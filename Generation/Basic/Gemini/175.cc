#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

int main (int argc, char *argv[])
{
    // Enable logging for UDP applications
    LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create three nodes: one AP and two STAs
    NodeContainer nodes;
    nodes.Create (3);
    Ptr<Node> apNode = nodes.Get (0);
    Ptr<Node> staNode1 = nodes.Get (1);
    Ptr<Node> staNode2 = nodes.Get (2);

    // Install mobility model for all nodes
    // Using ConstantPositionMobilityModel for fixed positions.
    MobilityHelper mobility;
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.Install (nodes);

    // Set fixed positions for NetAnim visualization later
    // These will be overridden by AnimationInterface::SetConstantPosition
    // for NetAnim, but good to have them defined for internal simulation use too.

    // Configure Wi-Fi standards and channels
    WifiHelper wifi;
    wifi.SetStandard (WIFI_STANDARD_80211n_5GHZ); // Choose 802.11n in 5GHz band
    wifi.SetRemoteStationManager ("ns3::AarfWifiManager"); // Automatic Rate Fallback

    YansWifiChannelHelper channel;
    channel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
    channel.AddPropagationLoss ("ns3::FriisPropagationLossModel"); // Simple path loss model

    Ptr<YansWifiChannel> wifiChannel = channel.CreatePropagationLossAndChannel ();

    YansWifiPhyHelper phy;
    phy.SetChannel (wifiChannel);
    // Setting PcapDataLinkType for Wi-Fi tracing for tools like Wireshark
    phy.SetPcapDataLinkType (WifiPhyHelper::DLT_IEEE802_11_RADIO);

    // Configure MAC layer for AP and STAs
    NqosWifiMacHelper mac = NqosWifiMacHelper::Default ();
    Ssid ssid = Ssid ("ns-3-ssid"); // Common SSID for the network

    // Setup AP device
    mac.SetType ("ns3::ApWifiMac",
                 "Ssid", SsidValue (ssid),
                 "BeaconInterval", TimeValue (MicroSeconds (102400)));
    NetDeviceContainer apDevices = wifi.Install (phy, mac, apNode);

    // Setup STA devices
    mac.SetType ("ns3::StaWifiMac",
                 "Ssid", SsidValue (ssid),
                 "ActiveProbing", BooleanValue (false)); // STA doesn't actively probe for AP here
    NetDeviceContainer staDevices = wifi.Install (phy, mac, NodeContainer (staNode1, staNode2));

    // Install Internet stack on all nodes
    InternetStackHelper stack;
    stack.Install (nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase ("10.0.0.0", "255.255.255.0"); // Assign addresses from 10.0.0.x network

    Ipv4InterfaceContainer apInterfaces = address.Assign (apDevices);
    Ipv4InterfaceContainer staInterfaces = address.Assign (staDevices);

    // Setup applications: UDP Echo Client on staNode1 and Server on staNode2
    // Client (staNode1) will send packets to Server (staNode2) via AP
    UdpEchoServerHelper echoServer (9); // Port 9
    ApplicationContainer serverApps = echoServer.Install (staNode2);
    serverApps.Start (Seconds (1.0));
    serverApps.Stop (Seconds (10.0));

    // Get the IP address of staNode2 for the client to target
    Ipv4Address sta2IpAddress = staInterfaces.GetAddress (1); // staDevices.Get(0) is staNode1, Get(1) is staNode2

    UdpEchoClientHelper echoClient (sta2IpAddress, 9);
    echoClient.SetAttribute ("MaxPackets", UintegerValue (5));
    echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
    echoClient.SetAttribute ("PacketSize", UintegerValue (1024));
    ApplicationContainer clientApps = echoClient.Install (staNode1);
    clientApps.Start (Seconds (2.0));
    clientApps.Stop (Seconds (10.0));

    // Enable NetAnim visualization
    AnimationInterface anim ("wifi-ap-sta.xml");
    anim.SetConstantPosition (apNode, 0.0, 0.0);
    anim.SetConstantPosition (staNode1, 10.0, 0.0);
    anim.SetConstantPosition (staNode2, 0.0, 10.0); // Create a triangular layout

    // You can set node colors or sizes for better visualization if needed
    // anim.SetNodeColor (apNode, 0, 0, 255); // Blue for AP
    // anim.SetNodeColor (staNode1, 0, 255, 0); // Green for STA1
    // anim.SetNodeColor (staNode2, 255, 0, 0); // Red for STA2

    // Set simulation stop time
    Simulator::Stop (Seconds (11.0));

    // Run the simulation
    Simulator::Run ();
    Simulator::Destroy ();

    return 0;
}