/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/wifi-module.h"
#include "ns3/ssid.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/icmpv4-l4-protocol.h"
#include "ns3/packet-sink.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiCompressedBlockAckExample");

int
main(int argc, char *argv[])
{
  // Enable logging if desired
  // LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
  // LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);

  uint32_t maxPacketCount = 30;
  Time interPacketInterval = MilliSeconds(10);
  std::string dataRate = "10Mbps";
  uint16_t udpPort = 4000;

  CommandLine cmd;
  cmd.AddValue ("maxPacketCount", "Number of packets to send", maxPacketCount);
  cmd.AddValue ("interPacketInterval", "Interval between packets", interPacketInterval);
  cmd.AddValue ("dataRate", "CBR traffic rate", dataRate);
  cmd.Parse (argc, argv);

  // Create nodes: 1 AP + 2 STAs
  NodeContainer wifiStaNodes;
  wifiStaNodes.Create(2);
  NodeContainer wifiApNode;
  wifiApNode.Create(1);

  // Setup Wi-Fi channel and physical layer
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());

  // Use 802.11n standard
  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211n_2_4GHZ);

  // Set up MAC to use Compressed Block Ack for QoS
  WifiMacHelper mac;
  Ssid ssid = Ssid("ns3-80211n");

  // Station MAC setup
  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid),
              "QosSupported", BooleanValue(true));

  NetDeviceContainer staDevices = wifi.Install(phy, mac, wifiStaNodes);

  // Access Point MAC setup
  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid),
              "QosSupported", BooleanValue(true));

  NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiApNode);

  // Activate compressed block ack and adjust parameters on all Wi-Fi net devices
  // We change BlockAckInactivityTimeout and BlockAckThreshold for QoS
  Config::SetDefault("ns3::StaWifiMac::BE_MaxAmsduSize", UintegerValue(7935));
  Config::SetDefault("ns3::WifiRemoteStationManager::BlockAckThreshold", UintegerValue(8));
  Config::SetDefault("ns3::WifiRemoteStationManager::BlockAckInactivityTimeout", TimeValue(MilliSeconds(50)));
  // Compressed block ack is default in 802.11n, but double-check
  Config::SetDefault("ns3::WifiRemoteStationManager::BlockAckType", EnumValue(WIFI_BLOCK_ACK_COMPRESSED));

  // Add mobility
  MobilityHelper mobility;

  // Place the two STAs
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(3.0),
                                "DeltaY", DoubleValue(5.0),
                                "GridWidth", UintegerValue(2),
                                "LayoutType", StringValue("RowFirst"));

  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install(wifiStaNodes);

  // Place AP
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (1.5),
                                 "MinY", DoubleValue (10.0),
                                 "DeltaX", DoubleValue (0.0),
                                 "DeltaY", DoubleValue (0.0),
                                 "GridWidth", UintegerValue (1),
                                 "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install(wifiApNode);

  // Install Internet Stack and assign IPs
  InternetStackHelper stack;
  stack.Install(wifiApNode);
  stack.Install(wifiStaNodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer staInterfaces, apInterface;
  staInterfaces = address.Assign(staDevices);
  apInterface = address.Assign(apDevice);

  // UDP CLIENT: STA 0 sends to AP
  UdpClientHelper udpClient(apInterface.GetAddress(0), udpPort);
  udpClient.SetAttribute("MaxPackets", UintegerValue(maxPacketCount));
  udpClient.SetAttribute("Interval", TimeValue(interPacketInterval));
  udpClient.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApp = udpClient.Install(wifiStaNodes.Get(0));
  clientApp.Start(Seconds(1.0));
  clientApp.Stop(Seconds(5.0));

  // UDP SERVER (sink for CBR): AP listens
  UdpServerHelper udpServer(udpPort);
  ApplicationContainer serverApp = udpServer.Install(wifiApNode.Get(0));
  serverApp.Start(Seconds(0.0));
  serverApp.Stop(Seconds(6.0));

  // ICMP REPLY: AP replies with ping to STA (simulate ICMP using V4PingHelper)
  V4PingHelper icmpPing(staInterfaces.GetAddress(0));
  icmpPing.SetAttribute("Verbose", BooleanValue(true));
  icmpPing.SetAttribute("Interval", TimeValue(Seconds(0.1)));
  icmpPing.SetAttribute("Size", UintegerValue(64));
  icmpPing.SetAttribute("Count", UintegerValue(maxPacketCount));
  ApplicationContainer pingApps = icmpPing.Install(wifiApNode.Get(0));
  pingApps.Start(Seconds(1.5));
  pingApps.Stop(Seconds(6.0));

  // Enable routing
  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // Enable pcap tracing on the access point
  phy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);
  phy.EnablePcap("compressed-block-ack-ap", apDevice.Get(0), true, true);

  // Also enable for STA 0 (optional)
  // phy.EnablePcap("compressed-block-ack-sta0", staDevices.Get(0), true, true);

  Simulator::Stop(Seconds(7.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}