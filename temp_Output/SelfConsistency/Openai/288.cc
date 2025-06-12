#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/ssid.h"
#include "ns3/wifi-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("Wifi80211acTcpExample");

int
main (int argc, char *argv[])
{
  // Enable logging if needed
  // LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);

  // Simulation parameters
  uint32_t numSta = 2;
  double simTime = 10.0; // seconds
  std::string phyMode = "VhtMcs9"; // Max MCS for 802.11ac (VHT)
  std::string dataRate = "100Mbps";
  uint16_t tcpPort = 5000;

  // 1. Create nodes: 1 AP, 2 STAs
  NodeContainer wifiStaNodes;
  wifiStaNodes.Create (numSta);
  NodeContainer wifiApNode;
  wifiApNode.Create (1);

  // 2. Wifi setup: YansWifiPhy for physical layer
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  // 3. Wifi standard/ac configs
  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211ac);
  WifiMacHelper mac;
  Ssid ssid = Ssid ("ns3-80211ac");

  // 4. VHT Wi-Fi setup for 802.11ac
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                               "DataMode", StringValue (phyMode),
                               "ControlMode", StringValue ("VhtMcs0"));

  // 5. Setup STA devices
  NetDeviceContainer staDevices;
  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid));
  staDevices = wifi.Install (phy, mac, wifiStaNodes);

  // 6. Setup AP device
  NetDeviceContainer apDevice;
  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid));
  apDevice = wifi.Install (phy, mac, wifiApNode);

  // 7. Mobility
  MobilityHelper mobility;
  // Station mobility: RandomWalk2d within 50x50 area
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
      "MinX", DoubleValue (0.0),
      "MinY", DoubleValue (0.0),
      "DeltaX", DoubleValue (10.0),
      "DeltaY", DoubleValue (10.0),
      "GridWidth", UintegerValue (2),
      "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
    "Bounds", RectangleValue (Rectangle (0, 50, 0, 50)));
  mobility.Install (wifiStaNodes);

  // AP is stationary at center (25,25)
  MobilityHelper apMobility;
  apMobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  apMobility.Install (wifiApNode);
  Ptr<MobilityModel> apMob = wifiApNode.Get (0)->GetObject<MobilityModel> ();
  apMob->SetPosition (Vector (25.0, 25.0, 0.0));

  // 8. Internet stack + IP addresses
  InternetStackHelper stack;
  stack.Install (wifiApNode);
  stack.Install (wifiStaNodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer staInterfaces = address.Assign (staDevices);
  Ipv4InterfaceContainer apInterface = address.Assign (apDevice);

  // 9. Applications
  // STA 0: acts as server; STA 1: acts as client
  uint32_t serverNodeIdx = 0; // STA0 = server
  uint32_t clientNodeIdx = 1; // STA1 = client

  // 9.1. Install TCP Server (PacketSink) on STA 0
  Address sinkLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), tcpPort));
  PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", sinkLocalAddress);
  ApplicationContainer serverApp = packetSinkHelper.Install (wifiStaNodes.Get (serverNodeIdx));
  serverApp.Start (Seconds (0.0));
  serverApp.Stop (Seconds (simTime));

  // 9.2. Install TCP Client on STA 1
  OnOffHelper clientHelper ("ns3::TcpSocketFactory",
                            InetSocketAddress (staInterfaces.GetAddress (serverNodeIdx), tcpPort));
  clientHelper.SetAttribute ("DataRate", DataRateValue (DataRate (dataRate)));
  clientHelper.SetAttribute ("PacketSize", UintegerValue (1024));
  clientHelper.SetAttribute ("StartTime", TimeValue (Seconds (1.0)));
  clientHelper.SetAttribute ("StopTime", TimeValue (Seconds (simTime)));

  ApplicationContainer clientApp = clientHelper.Install (wifiStaNodes.Get (clientNodeIdx));

  // 10. Enable PCAP tracing
  phy.SetPcapDataLinkType (YansWifiPhyHelper::DLT_IEEE802_11_RADIO);
  phy.EnablePcap ("wifi-80211ac-ap", apDevice.Get (0));
  phy.EnablePcap ("wifi-80211ac-sta0", staDevices.Get (0));
  phy.EnablePcap ("wifi-80211ac-sta1", staDevices.Get (1));

  // 11. Run simulation
  Simulator::Stop (Seconds (simTime));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}