#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/ssid.h"

// NS_LOG_COMPONENT_DEFINE ("WifiSimpleUdp"); // Omitted as per instructions: no commentary

int main (int argc, char *argv[])
{
  // Set default parameters for Wi-Fi
  Config::SetDefault ("ns3::WifiRemoteStationManager::FragmentationThreshold", StringValue ("-1"));
  Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue ("-1"));
  Config::SetDefault ("ns3::ChannelSettings::MaxLoss", StringValue("0")); // Default max loss is not 0 for some scenarios

  // 1. Create Nodes
  NodeContainer wifiStaNodes;
  wifiStaNodes.Create (3); // Three STAs
  NodeContainer wifiApNode;
  wifiApNode.Create (1); // One AP

  // 2. Configure Mobility
  MobilityHelper mobility;

  // AP Mobility: Static at (0,0,0)
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (wifiApNode);
  wifiApNode.Get (0)->GetObject<ConstantPositionMobilityModel> ()->SetPosition (Vector (0.0, 0.0, 0.0));

  // STA Mobility: Grid layout (static positions)
  Ptr<GridPositionAllocator> positionAlloc = CreateObject<GridPositionAllocator> ();
  positionAlloc->SetMinX (1.0); // Start X for first STA
  positionAlloc->SetMinY (0.0); // Start Y for STAs (on the same line as AP for simplicity)
  positionAlloc->SetDeltaX (1.0); // Distance between STAs in X
  positionAlloc->SetDeltaY (0.0); // No Y-offset between STAs
  positionAlloc->SetGridWidth (3); // Place 3 nodes in a row
  positionAlloc->SetLayoutType (GridPositionAllocator::ROW_FIRST);

  mobility.SetPositionAllocator (positionAlloc);
  mobility.Install (wifiStaNodes); // Install constant position mobility for STAs with grid positions

  // 3. Configure Wi-Fi Devices
  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211n_5GHZ); // IEEE 802.11n 5GHz

  YansWifiChannelHelper channel;
  channel.SetPropagationLoss ("ns3::FriisPropagationLossModel", "Frequency", DoubleValue(5.18e9)); // 5GHz
  channel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
  Ptr<YansWifiChannel> wifiChannel = channel.Create ();

  YansWifiPhyHelper phy;
  phy.SetChannel (wifiChannel);
  phy.Set ("RxGain", DoubleValue (-10)); 

  WifiMacHelper mac;
  Ssid ssid = Ssid ("ns3-wifi");

  // Configure STA devices
  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false)); // Passive scans are faster for setup
  NetDeviceContainer staDevices;
  staDevices = wifi.Install (phy, mac, wifiStaNodes);

  // Configure AP device
  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid));
  NetDeviceContainer apDevice;
  apDevice = wifi.Install (phy, mac, wifiApNode);

  // 4. Install Internet Stack
  InternetStackHelper stack;
  stack.Install (wifiStaNodes);
  stack.Install (wifiApNode);

  // 5. Assign IP Addresses
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer staInterfaces = ipv4.Assign (staDevices);
  Ipv4InterfaceContainer apInterface = ipv4.Assign (apDevice);

  // Get AP's IP address for clients to send to
  Ipv4Address apIpAddress = apInterface.GetAddress (0);
  uint16_t sinkPort = 9; // UDP sink port

  // 6. Create Simplex UDP Server on AP
  PacketSinkHelper sinkHelper ("ns3::UdpSocketFactory",
                               InetSocketAddress (Ipv4Address::GetAny (), sinkPort));
  ApplicationContainer serverApps = sinkHelper.Install (wifiApNode);
  serverApps.Start (Seconds (0.0));
  serverApps.Stop (Seconds (10.0));

  // 7. Create Simplex UDP Clients on STAs
  uint32_t packetSize = 512;
  uint32_t numPackets = 5;
  Time interPacketInterval = Seconds (1.0); // 1-second intervals

  for (uint32_t i = 0; i < wifiStaNodes.GetN (); ++i)
    {
      UdpClientHelper clientHelper (apIpAddress, sinkPort);
      clientHelper.SetAttribute ("MaxPackets", UintegerValue (numPackets));
      clientHelper.SetAttribute ("Interval", TimeValue (interPacketInterval));
      clientHelper.SetAttribute ("PacketSize", UintegerValue (packetSize));

      ApplicationContainer clientApps = clientHelper.Install (wifiStaNodes.Get (i));
      // Stagger client start times slightly to avoid simultaneous bursts if not desired.
      // E.g., STA0 starts at 1s, STA1 at 1.1s, STA2 at 1.2s.
      clientApps.Start (Seconds (1.0 + 0.1 * i));
      clientApps.Stop (Seconds (10.0));
    }

  // 8. Set up tracing (optional, omitted as per instructions)
  // phy.EnablePcap ("wifi-sta", staDevices);
  // phy.EnablePcap ("wifi-ap", apDevice);

  // 9. Run Simulation
  Simulator::Stop (Seconds (10.0)); // Simulation runs for 10 seconds
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}