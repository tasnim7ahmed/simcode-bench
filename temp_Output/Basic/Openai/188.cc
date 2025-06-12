#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiStaApUdpNetAnimExample");

uint32_t g_rxPackets = 0;
uint32_t g_txPackets = 0;
uint32_t g_rxBytes = 0;

void RxPacketCounter(Ptr<const Packet> packet, const Address &address)
{
  g_rxPackets++;
  g_rxBytes += packet->GetSize();
}

void TxPacketCounter(Ptr<const Packet> packet)
{
  g_txPackets++;
}

int main (int argc, char *argv[])
{
  uint32_t packetSize = 1024;
  double simulationTime = 10.0;
  uint32_t numSta = 2;
  DataRate udpDataRate ("10Mbps");

  CommandLine cmd;
  cmd.Parse (argc, argv);

  // Create nodes: 2 STA + 1 AP
  NodeContainer wifiStaNodes;
  wifiStaNodes.Create (numSta);
  NodeContainer wifiApNode;
  wifiApNode.Create (1);

  // Create WiFi channel and devices
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211g);
  wifi.SetRemoteStationManager ("ns3::AarfWifiManager");

  WifiMacHelper mac;
  Ssid ssid = Ssid ("ns3-wifi");

  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));
  NetDeviceContainer staDevices = wifi.Install (phy, mac, wifiStaNodes);

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid));
  NetDeviceContainer apDevice = wifi.Install (phy, mac, wifiApNode);

  // Mobility
  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                "MinX", DoubleValue (0.0),
                                "MinY", DoubleValue (0.0),
                                "DeltaX", DoubleValue (10.0),
                                "DeltaY", DoubleValue (10.0),
                                "GridWidth", UintegerValue (2),
                                "LayoutType", StringValue ("RowFirst"));

  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (wifiStaNodes);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (wifiApNode);

  // Install Internet stack
  InternetStackHelper stack;
  stack.Install (wifiStaNodes);
  stack.Install (wifiApNode);

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer staInterfaces;
  staInterfaces = address.Assign (staDevices);
  Ipv4InterfaceContainer apInterface;
  apInterface = address.Assign (apDevice);

  // UDP server on STA2 (index 1)
  uint16_t port = 4000;
  UdpServerHelper udpServer (port);
  ApplicationContainer serverApp = udpServer.Install (wifiStaNodes.Get (1));
  serverApp.Start (Seconds (0.0));
  serverApp.Stop (Seconds (simulationTime));

  // UDP client on STA1 (index 0) sending to STA2
  UdpClientHelper udpClient (staInterfaces.GetAddress (1), port);
  udpClient.SetAttribute ("MaxPackets", UintegerValue (10000));
  udpClient.SetAttribute ("Interval", TimeValue (Seconds (packetSize * 8.0 / (double) udpDataRate.GetBitRate ())));
  udpClient.SetAttribute ("PacketSize", UintegerValue (packetSize));

  ApplicationContainer clientApp = udpClient.Install (wifiStaNodes.Get (0));
  clientApp.Start (Seconds (1.0));
  clientApp.Stop (Seconds (simulationTime));

  // Packet counters for throughput/packet loss analysis
  Ptr<NetDevice> dev0 = staDevices.Get (0);
  dev0->TraceConnectWithoutContext ("PhyTxEnd", MakeCallback (&TxPacketCounter));
  Ptr<NetDevice> dev1 = staDevices.Get (1);
  dev1->TraceConnectWithoutContext ("PhyRxEnd", MakeCallback (&RxPacketCounter));

  // NetAnim
  AnimationInterface anim ("wifi-netanim.xml");
  anim.SetConstantPosition (wifiStaNodes.Get (0), 10, 20);
  anim.SetConstantPosition (wifiStaNodes.Get (1), 30, 20);
  anim.SetConstantPosition (wifiApNode.Get (0), 20, 10);

  Simulator::Stop (Seconds (simulationTime));
  Simulator::Run ();

  // Metrics
  uint32_t totalTx = g_txPackets;
  uint32_t totalRx = g_rxPackets;
  uint32_t totalRxBytes = g_rxBytes;
  double throughput = totalRxBytes * 8.0 / simulationTime / 1e6; // in Mbps
  uint32_t pktLoss = (totalTx > totalRx) ? (totalTx - totalRx) : 0;

  std::cout << "Simulation Results:" << std::endl;
  std::cout << " Total Packets Sent: " << totalTx << std::endl;
  std::cout << " Total Packets Received: " << totalRx << std::endl;
  std::cout << " Packet Loss: " << pktLoss << std::endl;
  std::cout << " Throughput: " << throughput << " Mbps" << std::endl;

  Simulator::Destroy ();
  return 0;
}