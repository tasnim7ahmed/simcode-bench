#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/command-line.h"
#include "ns3/uinteger.h"
#include "ns3/double.h"
#include "ns3/string.h"
#include "ns3/txop.h"
#include "ns3/config.h"
#include "ns3/qos-txop.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiQosExample");

int main (int argc, char *argv[])
{
  bool verbose = false;
  bool tracing = false;
  double simulationTime = 10;
  std::string payloadSize = "1472";
  double distance = 10.0;
  bool rtsCts = false;
  bool pcapTracing = false;

  CommandLine cmd;
  cmd.AddValue ("verbose", "Tell application containers to log if set to true.", verbose);
  cmd.AddValue ("tracing", "Enable pcap tracing", tracing);
  cmd.AddValue ("simulationTime", "Simulation time in seconds.", simulationTime);
  cmd.AddValue ("payloadSize", "Payload size of UDP packets", payloadSize);
  cmd.AddValue ("distance", "Distance between AP and STA", distance);
  cmd.AddValue ("rtsCts", "Enable RTS/CTS", rtsCts);
  cmd.AddValue ("pcapTracing", "Enable pcap tracing", pcapTracing);
  cmd.Parse (argc, argv);

  Config::SetDefault ("ns3::WifiMacQueue::MaxSize", StringValue ("65535p"));

  if (verbose)
    {
      LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
      LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);
    }

  NodeContainer apNodes;
  apNodes.Create (4);

  NodeContainer staNodes;
  staNodes.Create (4);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211n);

  NqosWifiMacHelper mac;
  Ssid ssid1 = Ssid ("ns3-wifi-qos-1");
  Ssid ssid2 = Ssid ("ns3-wifi-qos-2");
  Ssid ssid3 = Ssid ("ns3-wifi-qos-3");
  Ssid ssid4 = Ssid ("ns3-wifi-qos-4");

  NetDeviceContainer apDevices1;
  NetDeviceContainer staDevices1;
  NetDeviceContainer apDevices2;
  NetDeviceContainer staDevices2;
  NetDeviceContainer apDevices3;
  NetDeviceContainer staDevices3;
  NetDeviceContainer apDevices4;
  NetDeviceContainer staDevices4;

  mac.SetType ("ns3::ApWifiMac",
                 "Ssid", SsidValue (ssid1));
  apDevices1 = wifi.Install (phy, mac, apNodes.Get (0));

  mac.SetType ("ns3::StationWifiMac",
                 "Ssid", SsidValue (ssid1),
                 "ActiveProbing", BooleanValue (false));
  staDevices1 = wifi.Install (phy, mac, staNodes.Get (0));

  mac.SetType ("ns3::ApWifiMac",
                 "Ssid", SsidValue (ssid2));
  apDevices2 = wifi.Install (phy, mac, apNodes.Get (1));

  mac.SetType ("ns3::StationWifiMac",
                 "Ssid", SsidValue (ssid2),
                 "ActiveProbing", BooleanValue (false));
  staDevices2 = wifi.Install (phy, mac, staNodes.Get (1));

  mac.SetType ("ns3::ApWifiMac",
                 "Ssid", SsidValue (ssid3));
  apDevices3 = wifi.Install (phy, mac, apNodes.Get (2));

  mac.SetType ("ns3::StationWifiMac",
                 "Ssid", SsidValue (ssid3),
                 "ActiveProbing", BooleanValue (false));
  staDevices3 = wifi.Install (phy, mac, staNodes.Get (2));

  mac.SetType ("ns3::ApWifiMac",
                 "Ssid", SsidValue (ssid4));
  apDevices4 = wifi.Install (phy, mac, apNodes.Get (3));

  mac.SetType ("ns3::StationWifiMac",
                 "Ssid", SsidValue (ssid4),
                 "ActiveProbing", BooleanValue (false));
  staDevices4 = wifi.Install (phy, mac, staNodes.Get (3));

  if (rtsCts)
    {
      Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/RtsCtsThreshold", StringValue ("0"));
    }

  MobilityHelper mobility;

  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (distance),
                                 "DeltaY", DoubleValue (distance),
                                 "GridWidth", UintegerValue (4),
                                 "LayoutType", StringValue ("RowFirst"));

  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");

  mobility.Install (apNodes);
  mobility.Install (staNodes);

  InternetStackHelper stack;
  stack.Install (apNodes);
  stack.Install (staNodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterfaces1 = address.Assign (apDevices1);
  Ipv4InterfaceContainer staInterfaces1 = address.Assign (staDevices1);

  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterfaces2 = address.Assign (apDevices2);
  Ipv4InterfaceContainer staInterfaces2 = address.Assign (staDevices2);

  address.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterfaces3 = address.Assign (apDevices3);
  Ipv4InterfaceContainer staInterfaces3 = address.Assign (staDevices3);

  address.SetBase ("10.1.4.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterfaces4 = address.Assign (apDevices4);
  Ipv4InterfaceContainer staInterfaces4 = address.Assign (staDevices4);

  UdpServerHelper echoServer (9);
  ApplicationContainer serverApps1 = echoServer.Install (apNodes.Get (0));
  serverApps1.Start (Seconds (0.0));
  serverApps1.Stop (Seconds (simulationTime));

  ApplicationContainer serverApps2 = echoServer.Install (apNodes.Get (1));
  serverApps2.Start (Seconds (0.0));
  serverApps2.Stop (Seconds (simulationTime));

  ApplicationContainer serverApps3 = echoServer.Install (apNodes.Get (2));
  serverApps3.Start (Seconds (0.0));
  serverApps3.Stop (Seconds (simulationTime));

  ApplicationContainer serverApps4 = echoServer.Install (apNodes.Get (3));
  serverApps4.Start (Seconds (0.0));
  serverApps4.Stop (Seconds (simulationTime));

  OnOffHelper onoff1 ("ns3::UdpClient", Address (InetSocketAddress (apInterfaces1.GetAddress (0), 9)));
  onoff1.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  onoff1.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
  onoff1.SetAttribute ("PacketSize", StringValue (payloadSize));
  onoff1.SetAttribute ("DataRate", StringValue ("100Mb/s"));
  ApplicationContainer clientApps1 = onoff1.Install (staNodes.Get (0));
  clientApps1.Start (Seconds (1.0));
  clientApps1.Stop (Seconds (simulationTime));

  TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
  Ptr<Socket> sock1 = Socket::CreateSocket (staNodes.Get (0), tid);
  sock1->SetAttribute ("Tos", UintegerValue (0xb8)); // AC_BE
  sock1->Bind ();
  sock1->Connect (InetSocketAddress (apInterfaces1.GetAddress (0), 9));

  OnOffHelper onoff2 ("ns3::UdpClient", Address (InetSocketAddress (apInterfaces2.GetAddress (0), 9)));
  onoff2.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  onoff2.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
  onoff2.SetAttribute ("PacketSize", StringValue (payloadSize));
  onoff2.SetAttribute ("DataRate", StringValue ("100Mb/s"));
  ApplicationContainer clientApps2 = onoff2.Install (staNodes.Get (1));
  clientApps2.Start (Seconds (1.0));
  clientApps2.Stop (Seconds (simulationTime));

  Ptr<Socket> sock2 = Socket::CreateSocket (staNodes.Get (1), tid);
  sock2->SetAttribute ("Tos", UintegerValue (0xe0)); // AC_VI
  sock2->Bind ();
  sock2->Connect (InetSocketAddress (apInterfaces2.GetAddress (0), 9));

    OnOffHelper onoff3 ("ns3::UdpClient", Address (InetSocketAddress (apInterfaces3.GetAddress (0), 9)));
  onoff3.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  onoff3.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
  onoff3.SetAttribute ("PacketSize", StringValue (payloadSize));
  onoff3.SetAttribute ("DataRate", StringValue ("100Mb/s"));
  ApplicationContainer clientApps3 = onoff3.Install (staNodes.Get (2));
  clientApps3.Start (Seconds (1.0));
  clientApps3.Stop (Seconds (simulationTime));

  Ptr<Socket> sock3 = Socket::CreateSocket (staNodes.Get (2), tid);
  sock3->SetAttribute ("Tos", UintegerValue (0xb8)); // AC_BE
  sock3->Bind ();
  sock3->Connect (InetSocketAddress (apInterfaces3.GetAddress (0), 9));

    OnOffHelper onoff4 ("ns3::UdpClient", Address (InetSocketAddress (apInterfaces4.GetAddress (0), 9)));
  onoff4.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  onoff4.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
  onoff4.SetAttribute ("PacketSize", StringValue (payloadSize));
  onoff4.SetAttribute ("DataRate", StringValue ("100Mb/s"));
  ApplicationContainer clientApps4 = onoff4.Install (staNodes.Get (3));
  clientApps4.Start (Seconds (1.0));
  clientApps4.Stop (Seconds (simulationTime));

  Ptr<Socket> sock4 = Socket::CreateSocket (staNodes.Get (3), tid);
  sock4->SetAttribute ("Tos", UintegerValue (0xe0)); // AC_VI
  sock4->Bind ();
  sock4->Connect (InetSocketAddress (apInterfaces4.GetAddress (0), 9));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  if (pcapTracing)
    {
      phy.EnablePcap ("wifi-qos", apDevices1.Get (0));
      phy.EnablePcap ("wifi-qos", staDevices1.Get (0));
      phy.EnablePcap ("wifi-qos", apDevices2.Get (0));
      phy.EnablePcap ("wifi-qos", staDevices2.Get (0));
      phy.EnablePcap ("wifi-qos", apDevices3.Get (0));
      phy.EnablePcap ("wifi-qos", staDevices3.Get (0));
      phy.EnablePcap ("wifi-qos", apDevices4.Get (0));
      phy.EnablePcap ("wifi-qos", staDevices4.Get (0));
    }
  Simulator::Stop (Seconds (simulationTime));

  Simulator::Run ();

  monitor->CheckForLostPackets ();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();

  double throughput1 = 0.0;
  double throughput2 = 0.0;
  double throughput3 = 0.0;
  double throughput4 = 0.0;

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
      std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
      std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
      std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000 / 1000  << " Mbps\n";

      if (t.destinationAddress == apInterfaces1.GetAddress (0)) {
          throughput1 = i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000 / 1000;
      } else if (t.destinationAddress == apInterfaces2.GetAddress (0)) {
          throughput2 = i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000 / 1000;
      } else if (t.destinationAddress == apInterfaces3.GetAddress (0)) {
          throughput3 = i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000 / 1000;
      } else if (t.destinationAddress == apInterfaces4.GetAddress (0)) {
          throughput4 = i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000 / 1000;
      }
    }

  std::cout << "Throughput 1: " << throughput1 << " Mbps" << std::endl;
  std::cout << "Throughput 2: " << throughput2 << " Mbps" << std::endl;
  std::cout << "Throughput 3: " << throughput3 << " Mbps" << std::endl;
  std::cout << "Throughput 4: " << throughput4 << " Mbps" << std::endl;

  Simulator::Destroy ();
  return 0;
}