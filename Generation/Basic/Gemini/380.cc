#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"

// Forward declaration of Ssid for clarity, though not strictly needed here.
using ns3::Ssid;

int main (int argc, char *argv[])
{
  // 1. Create nodes
  NodeContainer nodes;
  nodes.Create (2);

  // 2. Install Mobility Model (ConstantPositionMobilityModel)
  // This places nodes at fixed positions to ensure connectivity and simplify setup.
  MobilityHelper mobility;
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes);

  // Position Node 0 (Client) at (0,0,0)
  nodes.Get (0)->GetObject<ConstantPositionMobilityModel> ()->SetPosition (Vector (0.0, 0.0, 0.0));
  // Position Node 1 (Server) at (10,0,0)
  nodes.Get (1)->GetObject<ConstantPositionMobilityModel> ()->SetPosition (Vector (10.0, 0.0, 0.0));

  // 3. Install Internet Stack
  InternetStackHelper internet;
  internet.Install (nodes);

  // 4. Configure WiFi (802.11n at 5GHz)
  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211n_5GHZ); // Set 802.11n 5GHz standard

  HtWifiPhyHelper wifiPhy; // Use HtWifiPhyHelper for 802.11n (High Throughput)
  wifiPhy.SetChannel (YansWifiChannelHelper::Create ());
  wifiPhy.SetPcapDataLinkType (WifiPhyHelper::DLT_IEEE802_11_RADIO);

  HtWifiMacHelper wifiMac; // Use HtWifiMacHelper for 802.11n

  Ssid ssid = Ssid ("ns3-wifi"); // Define an SSID for the network

  // Configure Node 0 as an Access Point (AP)
  NetDeviceContainer apDevice;
  wifiMac.SetType ("ns3::ApWifiMac",
                   "Ssid", SsidValue (ssid),
                   "EnableBeaconJitter", BooleanValue (false));
  apDevice = wifi.Install (wifiPhy, wifiMac, nodes.Get (0));

  // Configure Node 1 as a Station (STA)
  NetDeviceContainer staDevice;
  wifiMac.SetType ("ns3::StaWifiMac",
                   "Ssid", SsidValue (ssid));
  staDevice = wifi.Install (wifiPhy, wifiMac, nodes.Get (1));

  // 5. Assign IP Addresses
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0"); // Set network address and netmask
  Ipv4InterfaceContainer apIpIfaces = ipv4.Assign (apDevice);
  Ipv4InterfaceContainer staIpIfaces = ipv4.Assign (staDevice);

  // 6. Setup UDP Application
  uint16_t port = 9; // Echo port for UDP

  // Server (Node 2 - ns3 node index 1)
  // Node 2's IP address is staIpIfaces.GetAddress (0)
  PacketSinkHelper sinkHelper ("ns3::UdpSocketFactory",
                               InetSocketAddress (Ipv4Address::GetAny (), port));
  ApplicationContainer serverApps = sinkHelper.Install (nodes.Get (1)); // Install on Node 1 (Server)
  serverApps.Start (Seconds (1.0)); // Server starts at 1 second
  serverApps.Stop (Seconds (10.0)); // Server stops at 10 seconds

  // Client (Node 1 - ns3 node index 0)
  // Client sends to Node 2's IP: staIpIfaces.GetAddress (0)
  UdpClientHelper clientHelper (staIpIfaces.GetAddress (0), port);
  clientHelper.SetAttribute ("MaxPackets", UintegerValue (1000)); // Send a large number of packets
  clientHelper.SetAttribute ("Interval", TimeValue (MilliSeconds (100))); // Send a packet every 100ms
  clientHelper.SetAttribute ("PacketSize", UintegerValue (1024)); // Each packet is 1024 bytes
  ApplicationContainer clientApps = clientHelper.Install (nodes.Get (0)); // Install on Node 0 (Client)
  clientApps.Start (Seconds (2.0)); // Client starts at 2 seconds
  clientApps.Stop (Seconds (10.0)); // Client stops at 10 seconds

  // 7. Run Simulation
  Simulator::Stop (Seconds (10.0)); // Simulation runs for 10 seconds
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}