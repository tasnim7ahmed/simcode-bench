#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/ssid.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/packet-sink.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiTcpApStaExample");

uint32_t g_bytesReceived = 0;
uint32_t g_packetsReceived = 0;

void
PacketReceivedCallback (Ptr<const Packet> packet, const Address &addr)
{
  g_bytesReceived += packet->GetSize ();
  g_packetsReceived++;
}

int
main (int argc, char *argv[])
{
  // Set simulation time
  double simTime = 10.0;

  // Create nodes: 2 STAs + 1 AP
  NodeContainer wifiStaNodes;
  wifiStaNodes.Create (2);
  NodeContainer wifiApNode;
  wifiApNode.Create (1);

  // Setup Wi-Fi
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211b);
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                               "DataMode", StringValue ("DsssRate11Mbps"),
                               "ControlMode", StringValue ("DsssRate11Mbps"));

  WifiMacHelper mac;
  Ssid ssid = Ssid ("ns3-80211b-ssid");

  // STAs
  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));
  NetDeviceContainer staDevices;
  staDevices = wifi.Install (phy, mac, wifiStaNodes);

  // AP
  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid));
  NetDeviceContainer apDevices;
  apDevices = wifi.Install (phy, mac, wifiApNode);

  // Mobility
  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                "MinX", DoubleValue (0.0),
                                "MinY", DoubleValue (0.0),
                                "DeltaX", DoubleValue (5.0),
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
  Ipv4InterfaceContainer apInterfaces;
  apInterfaces = address.Assign (apDevices);

  // Set up TCP server (PacketSinkApp) on first STA (staNodes.Get(0))
  uint16_t sinkPort = 50000;
  Address sinkAddress (InetSocketAddress (staInterfaces.GetAddress (0), sinkPort));
  PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), sinkPort));
  ApplicationContainer sinkApp = packetSinkHelper.Install (wifiStaNodes.Get (0));
  sinkApp.Start (Seconds (0.0));
  sinkApp.Stop (Seconds (simTime + 1.0));

  // Set up TCP client (OnOffApplication) on second STA (staNodes.Get(1)), targeting first STA
  OnOffHelper clientHelper ("ns3::TcpSocketFactory", sinkAddress);
  clientHelper.SetAttribute ("DataRate", DataRateValue (DataRate ("11Mbps")));
  clientHelper.SetAttribute ("PacketSize", UintegerValue (1024));
  clientHelper.SetAttribute ("StartTime", TimeValue (Seconds (1.0)));
  clientHelper.SetAttribute ("StopTime", TimeValue (Seconds (simTime)));

  ApplicationContainer clientApp = clientHelper.Install (wifiStaNodes.Get (1));

  // Enable pcap tracing
  phy.SetPcapDataLinkType (WifiPhyHelper::DLT_IEEE802_11_RADIO);
  phy.EnablePcap ("ap", apDevices.Get (0));
  phy.EnablePcap ("sta-0", staDevices.Get (0));
  phy.EnablePcap ("sta-1", staDevices.Get (1));

  // Packet sink tracing to measure throughput and packet counts
  Ptr<PacketSink> sink = DynamicCast<PacketSink> (sinkApp.Get (0));
  sink->TraceConnectWithoutContext ("Rx", MakeCallback (&PacketReceivedCallback));

  Simulator::Stop (Seconds (simTime + 1.0));
  Simulator::Run ();

  double throughput = (g_bytesReceived * 8.0) / (simTime * 1000000.0); // Mbps

  std::cout << "Simulation complete.\n";
  std::cout << "  Packets received at server: " << g_packetsReceived << "\n";
  std::cout << "  Bytes received at server: " << g_bytesReceived << "\n";
  std::cout << "  Throughput at server: " << throughput << " Mbps\n";

  Simulator::Destroy ();
  return 0;
}