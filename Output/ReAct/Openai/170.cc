#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/ssid.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

uint32_t g_rxPackets = 0;
uint32_t g_rxBytes = 0;

void
RxPacketCounter (Ptr<const Packet> packet, const Address &from)
{
  g_rxPackets++;
  g_rxBytes += packet->GetSize ();
}

int
main (int argc, char *argv[])
{
  Time::SetResolution (Time::NS);

  NodeContainer wifiStaNodes;
  wifiStaNodes.Create (2);
  NodeContainer wifiApNode;
  wifiApNode.Create (1);

  // Configure Wi-Fi
  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211b);

  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.Set ("TxPowerStart", DoubleValue (16.0));
  phy.Set ("TxPowerEnd", DoubleValue (16.0));

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiMacHelper mac;
  Ssid ssid = Ssid ("ns3-80211b");

  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));
  NetDeviceContainer staDevices;
  staDevices = wifi.Install (phy, mac, wifiStaNodes);

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid));
  NetDeviceContainer apDevice;
  apDevice = wifi.Install (phy, mac, wifiApNode);

  // Mobility
  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                "MinX", DoubleValue (0.0),
                                "MinY", DoubleValue (0.0),
                                "DeltaX", DoubleValue (5.0),
                                "DeltaY", DoubleValue (0.0),
                                "GridWidth", UintegerValue (3),
                                "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (wifiStaNodes);
  mobility.Install (wifiApNode);

  // Internet stack
  InternetStackHelper stack;
  stack.Install (wifiStaNodes);
  stack.Install (wifiApNode);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer staInterfaces;
  staInterfaces = address.Assign (staDevices);
  Ipv4InterfaceContainer apInterface;
  apInterface = address.Assign (apDevice);

  // Set up applications
  uint16_t port = 50000;

  // Server (STA 1)
  Address serverAddress (InetSocketAddress (staInterfaces.GetAddress (1), port));
  PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory",
                                     InetSocketAddress (Ipv4Address::GetAny (), port));
  ApplicationContainer serverApp = packetSinkHelper.Install (wifiStaNodes.Get (1));
  serverApp.Start (Seconds (0.0));
  serverApp.Stop (Seconds (10.0));

  // Count received packets
  Ptr<PacketSink> sink = DynamicCast<PacketSink> (serverApp.Get (0));
  sink->TraceConnectWithoutContext ("Rx", MakeCallback (&RxPacketCounter));

  // TCP client (STA 0)
  OnOffHelper clientHelper ("ns3::TcpSocketFactory", serverAddress);
  clientHelper.SetAttribute ("DataRate", DataRateValue (DataRate ("5Mbps")));
  clientHelper.SetAttribute ("PacketSize", UintegerValue (1024));
  clientHelper.SetAttribute ("StartTime", TimeValue (Seconds (1.0)));
  clientHelper.SetAttribute ("StopTime", TimeValue (Seconds (10.0)));
  clientHelper.SetAttribute ("MaxBytes", UintegerValue (0)); // unlimited for duration

  ApplicationContainer clientApp = clientHelper.Install (wifiStaNodes.Get (0));

  // PCAP tracing
  phy.EnablePcap ("wifi-ap", apDevice, true);
  phy.EnablePcap ("wifi-sta0", staDevices.Get (0), true);
  phy.EnablePcap ("wifi-sta1", staDevices.Get (1), true);

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();

  // Throughput calculation
  double throughput = (g_rxBytes * 8.0) / (9.0 * 1000000.0); // in Mbps; traffic is from 1s to 10s
  std::cout << "Total Packets Received at Server: " << g_rxPackets << std::endl;
  std::cout << "Total Bytes Received at Server:   " << g_rxBytes << std::endl;
  std::cout << "Server Side Throughput:           " << throughput << " Mbps" << std::endl;

  Simulator::Destroy ();
  return 0;
}