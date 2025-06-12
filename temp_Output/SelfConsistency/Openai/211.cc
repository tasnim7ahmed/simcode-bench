/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/applications-module.h"
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiAdhocRandomMobility");

struct FlowStats
{
  uint32_t packetsSent = 0;
  uint32_t packetsReceived = 0;
  uint32_t bytesReceived = 0;
  Time firstRx = Seconds(0);
  Time lastRx = Seconds(0);
};

FlowStats g_flowStats;

void
TxAppTrace (Ptr<const Packet> packet)
{
  g_flowStats.packetsSent++;
}

void
RxAppTrace (Ptr<const Packet> packet, const Address &from)
{
  if (g_flowStats.packetsReceived == 0)
    {
      g_flowStats.firstRx = Simulator::Now();
    }
  g_flowStats.lastRx = Simulator::Now();
  g_flowStats.packetsReceived++;
  g_flowStats.bytesReceived += packet->GetSize();
}

int
main (int argc, char *argv[])
{
  LogComponentEnable ("WifiAdhocRandomMobility", LOG_LEVEL_INFO);

  uint32_t numNodes = 5;
  double simTime = 20.0; // seconds
  double distance = 75.0; // max node movement range

  NodeContainer nodes;
  nodes.Create (numNodes);

  // Configure WiFi
  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  wifiPhy.Set ("TxPowerStart", DoubleValue (16.0));
  wifiPhy.Set ("TxPowerEnd", DoubleValue (16.0));
  wifiPhy.Set ("RxGain", DoubleValue (0.0));
  wifiPhy.Set ("TxGain", DoubleValue (0.0));
  wifiPhy.Set ("EnergyDetectionThreshold", DoubleValue(-80.0));
  wifiPhy.Set ("CcaMode1Threshold", DoubleValue(-80.0));

  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  wifiPhy.SetChannel (wifiChannel.Create ());

  WifiMacHelper wifiMac;
  wifiMac.SetType ("ns3::AdhocWifiMac");

  NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, nodes);

  // Mobility - Random direction mobility model
  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::RandomRectanglePositionAllocator",
                                 "X", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=150.0]"),
                                 "Y", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=150.0]"));
  mobility.SetMobilityModel ("ns3::RandomDirection2dMobilityModel",
                             "Bounds", RectangleValue (Rectangle (0, 150, 0, 150)),
                             "Speed", StringValue ("ns3::UniformRandomVariable[Min=2.0|Max=5.0]"),
                             "Pause", StringValue ("ns3::ConstantRandomVariable[Constant=0.5]"));
  mobility.Install (nodes);

  // Internet stack and assign IP addresses
  InternetStackHelper internet;
  internet.Install (nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign (devices);

  // Install UDP application: node 0 sends to node 1
  uint16_t port = 4000;

  // UDP Server on node 1
  UdpServerHelper udpServer (port);
  ApplicationContainer serverApp = udpServer.Install (nodes.Get (1));
  serverApp.Start (Seconds (1.0));
  serverApp.Stop (Seconds (simTime));

  // UdpClient on node 0, sends to node 1
  uint32_t packetSize = 1024;
  uint32_t numPackets = 1000; // Limit if needed, but let's use high enough for streaming
  double interval = 0.05; // 20 packets/sec

  UdpClientHelper udpClient (interfaces.GetAddress (1), port);
  udpClient.SetAttribute ("MaxPackets", UintegerValue (numPackets));
  udpClient.SetAttribute ("Interval", TimeValue (Seconds (interval)));
  udpClient.SetAttribute ("PacketSize", UintegerValue (packetSize));

  ApplicationContainer clientApp = udpClient.Install (nodes.Get (0));
  clientApp.Start (Seconds (2.0));
  clientApp.Stop (Seconds (simTime));

  // Trace sent and received packets
  Ptr<Socket> sourceSocket = Socket::CreateSocket (nodes.Get (0), UdpSocketFactory::GetTypeId ());
  sourceSocket->TraceConnectWithoutContext ("Tx", MakeCallback (&TxAppTrace));
  Ptr<UdpServer> server = DynamicCast<UdpServer> (serverApp.Get (0));
  server->TraceConnectWithoutContext ("Rx", MakeCallback (&RxAppTrace));

  Simulator::Stop (Seconds (simTime));
  Simulator::Run ();

  // After simulation: Calculate stats
  double throughput = 0.0;
  double packetLossRate = 0.0;
  if (g_flowStats.firstRx != Seconds(0) && g_flowStats.lastRx > g_flowStats.firstRx && g_flowStats.bytesReceived > 0)
    {
      throughput = (g_flowStats.bytesReceived * 8.0) / (g_flowStats.lastRx - g_flowStats.firstRx).GetSeconds () / 1000.0; // kbps
    }
  if (g_flowStats.packetsSent > 0)
    {
      packetLossRate = (double)(g_flowStats.packetsSent - g_flowStats.packetsReceived) / g_flowStats.packetsSent;
    }

  // Write statistics to a file
  std::ofstream outFile;
  outFile.open ("adhoc-wifi-stats.txt");
  outFile << "UDP Flow from node 0 to node 1\n";
  outFile << "Packets sent: " << g_flowStats.packetsSent << "\n";
  outFile << "Packets received: " << g_flowStats.packetsReceived << "\n";
  outFile << "Bytes received: " << g_flowStats.bytesReceived << "\n";
  outFile << "Packet loss rate: " << packetLossRate * 100.0 << " %\n";
  outFile << "Throughput: " << throughput << " kbps\n";
  outFile.close ();

  NS_LOG_INFO ("Packets sent: " << g_flowStats.packetsSent);
  NS_LOG_INFO ("Packets received: " << g_flowStats.packetsReceived);
  NS_LOG_INFO ("Packet loss rate: " << packetLossRate * 100.0 << " %");
  NS_LOG_INFO ("Throughput: " << throughput << " kbps");

  Simulator::Destroy ();
  return 0;
}