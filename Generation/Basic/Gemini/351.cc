#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiSimpleUdp");

int
main (int argc, char *argv[])
{
  // Configure logging
  LogComponentEnable ("UdpClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpServerApplication", LOG_LEVEL_INFO);
  LogComponentEnable ("WifiPhy", LOG_LEVEL_INFO);
  LogComponentEnable ("WifiMac", LOG_LEVEL_INFO);

  // Set default Wi-Fi parameters
  // Ensure management frames use a basic rate
  Config::SetDefault ("ns3::WifiRemoteStationManager::NonUnicastMode", StringValue ("DsssRate1Mbps"));
  // Disable RTS/CTS for simplicity and small packets
  Config::SetDefault ("ns3::WifiMac::RtsCtsThreshold", UintegerValue (0));
  // Set default LongRetryLimit
  Config::SetDefault ("ns3::WifiMac::LongRetryLimit", UintegerValue (7));

  // Create nodes: AP and STA
  NodeContainer nodes;
  nodes.Create (2);

  Ptr<Node> apNode = nodes.Get (0);
  Ptr<Node> staNode = nodes.Get (1);

  // Configure mobility: Constant Position
  MobilityHelper mobility;
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes);

  // Set positions for AP and STA
  Ptr<ConstantPositionMobilityModel> apMobility = apNode->GetObject<ConstantPositionMobilityModel> ();
  apMobility->SetPosition (Vector (0.0, 0.0, 0.0)); // AP at origin

  Ptr<ConstantPositionMobilityModel> staMobility = staNode->GetObject<ConstantPositionMobilityModel> ();
  staMobility->SetPosition (Vector (1.0, 0.0, 0.0)); // STA slightly away from AP

  // Configure Wi-Fi channel and PHY
  YansWifiChannelHelper channel;
  channel.SetPropagationLossModel ("ns3::FriisPropagationLossModel");
  channel.SetPcapDataLinkType (WifiPhyHelper::DLT_IEEE802_11_RADIO);
  channel.AddPropagationLoss ("ns3::ConstantSpeedPropagationDelayModel");

  YansWifiPhyHelper phy;
  phy.SetChannel (channel.Create ());
  
  // Configure Wi-Fi MAC layer
  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211b); // Use 802.11b for basic setup

  WifiMacHelper mac;
  Ssid ssid = Ssid ("ns3-wifi");

  // Configure AP MAC type
  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid),
               "BeaconInterval", TimeValue (MicroSeconds (102400)));
  NetDeviceContainer apDevice;
  apDevice = wifi.Install (phy, mac, apNode);

  // Configure STA MAC type
  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false)); // STA passively scans for beacons
  NetDeviceContainer staDevice;
  staDevice = wifi.Install (phy, mac, staNode);

  // Install Internet Stack on nodes
  InternetStackHelper stack;
  stack.Install (nodes);

  // Assign IP addresses from 192.168.1.0/24 subnet
  Ipv4AddressHelper address;
  address.SetBase ("192.168.1.0", "255.255.255.0");
  
  // Assign IP to AP (first device installed gets 192.168.1.1)
  Ipv4InterfaceContainer apInterfaces = address.Assign (apDevice); // AP gets 192.168.1.1
  // Assign IP to STA (next device installed gets 192.168.1.2)
  Ipv4InterfaceContainer staInterfaces = address.Assign (staDevice);

  // Configure UDP Server on AP
  uint16_t serverPort = 8080;
  UdpServerHelper server (serverPort);
  ApplicationContainer serverApps = server.Install (apNode);
  serverApps.Start (Seconds (1.0)); // Server starts at 1 second
  serverApps.Stop (Seconds (10.0)); // Server runs until the end of simulation

  // Configure UDP Client on STA
  uint32_t packetSize = 512; // bytes
  uint32_t numPackets = 20;
  Time interPacketInterval = Seconds (0.5); // seconds

  // AP's IP address for the client to send to
  Ipv4Address remoteAddress = apInterfaces.GetAddress (0); 
  UdpClientHelper client (remoteAddress, serverPort);
  client.SetAttribute ("MaxPackets", UintegerValue (numPackets));
  client.SetAttribute ("Interval", TimeValue (interPacketInterval));
  client.SetAttribute ("PacketSize", UintegerValue (packetSize));
  ApplicationContainer clientApps = client.Install (staNode);
  clientApps.Start (Seconds (2.0)); // Client starts at 2 seconds
  // The client will attempt to send packets until MaxPackets is reached or simulation stops.
  // The last packet would be sent at 2.0 + (20-1)*0.5 = 11.5 seconds.
  // Since simulation stops at 10 seconds, not all 20 packets will be sent.
  // Setting client stop time to 10.0s aligns with simulation end.
  clientApps.Stop (Seconds (10.0)); 

  // Set simulation duration
  Simulator::Stop (Seconds (10.0));

  // Run simulation
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}