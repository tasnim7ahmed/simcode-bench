#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiTimingParamsDemo");

uint64_t totalBytesReceived = 0;

void
ReceivePacket (Ptr<const Packet> packet, const Address &)
{
  totalBytesReceived += packet->GetSize ();
}

int
main (int argc, char *argv[])
{
  double simulationTime = 10.0;

  // Timing parameters (default values for IEEE 802.11n 2.4GHz)
  uint32_t slotTime = 9;             // microseconds
  uint32_t sifsTime = 10;            // microseconds
  uint32_t pifsTime = slotTime + sifsTime; // PIFS = SIFS + SlotTime

  CommandLine cmd;
  cmd.AddValue ("slotTime", "Slot time in microseconds", slotTime);
  cmd.AddValue ("sifsTime", "SIFS time in microseconds", sifsTime);
  cmd.AddValue ("pifsTime", "PIFS time in microseconds", pifsTime);
  cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.Parse (argc, argv);

  NodeContainer wifiStaNode;
  wifiStaNode.Create (1);
  NodeContainer wifiApNode;
  wifiApNode.Create (1);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211n_2_4GHZ);

  WifiMacHelper mac;
  Ssid ssid = Ssid ("ns3-80211n");

  // Configure Wi-Fi timing parameters dynamically
  // These affect the physical DcfManager used by MACs
  Config::SetDefault ("ns3::DcfManager::SlotTime", TimeValue (MicroSeconds (slotTime)));
  Config::SetDefault ("ns3::DcfManager::Sifs", TimeValue (MicroSeconds (sifsTime)));
  Config::SetDefault ("ns3::DcfManager::Pifs", TimeValue (MicroSeconds (pifsTime)));

  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));
  NetDeviceContainer staDevice = wifi.Install (phy, mac, wifiStaNode);

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid));
  NetDeviceContainer apDevice = wifi.Install (phy, mac, wifiApNode);

  MobilityHelper mobility;

  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                "MinX", DoubleValue (0.0),
                                "MinY", DoubleValue (0.0),
                                "DeltaX", DoubleValue (5.0),
                                "DeltaY", DoubleValue (0.0),
                                "GridWidth", UintegerValue (2),
                                "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (wifiStaNode);
  mobility.Install (wifiApNode);

  InternetStackHelper stack;
  stack.Install (wifiStaNode);
  stack.Install (wifiApNode);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer staInterface, apInterface;
  staInterface = address.Assign (staDevice);
  apInterface = address.Assign (apDevice);

  uint16_t port = 5000;
  uint32_t maxPacketCount = 10000000;
  uint32_t packetSize = 1024; // bytes
  double interPacketInterval = 0.001; // seconds

  // UDP server on STA
  UdpServerHelper server (port);
  ApplicationContainer serverApp = server.Install (wifiStaNode.Get (0));
  serverApp.Start (Seconds (0.0));
  serverApp.Stop (Seconds (simulationTime));

  // UDP client on AP
  UdpClientHelper client (staInterface.GetAddress (0), port);
  client.SetAttribute ("MaxPackets", UintegerValue (maxPacketCount));
  client.SetAttribute ("Interval", TimeValue (Seconds (interPacketInterval)));
  client.SetAttribute ("PacketSize", UintegerValue (packetSize));
  ApplicationContainer clientApp = client.Install (wifiApNode.Get (0));
  clientApp.Start (Seconds (1.0));
  clientApp.Stop (Seconds (simulationTime));

  Simulator::Schedule (Seconds (0), &Config::Set, "/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/$ns3::RegularWifiMac/Dcf/DcaTxop/SlotTime", TimeValue (MicroSeconds (slotTime)));
  Simulator::Schedule (Seconds (0), &Config::Set, "/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/$ns3::RegularWifiMac/Dcf/DcaTxop/Sifs", TimeValue (MicroSeconds (sifsTime)));
  Simulator::Schedule (Seconds (0), &Config::Set, "/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/$ns3::RegularWifiMac/Dcf/DcaTxop/Pifs", TimeValue (MicroSeconds (pifsTime)));

  Simulator::Stop (Seconds (simulationTime));

  Simulator::Run();

  uint64_t totalBytes = DynamicCast<UdpServer> (serverApp.Get (0))->GetReceived ();
  double throughput = totalBytes * 8.0 / (simulationTime - 1.0) / 1e6; // Mbps, start after 1 s

  std::cout << "Simulation Parameters:" << std::endl;
  std::cout << "  SlotTime: " << slotTime << " us" << std::endl;
  std::cout << "  Sifs:     " << sifsTime << " us" << std::endl;
  std::cout << "  Pifs:     " << pifsTime << " us" << std::endl;
  std::cout << "Results:" << std::endl;
  std::cout << "  Received " << totalBytes << " bytes" << std::endl;
  std::cout << "  Throughput: " << throughput << " Mbps" << std::endl;

  Simulator::Destroy ();
  return 0;
}