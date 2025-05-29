#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/propagation-loss-model.h"
#include "ns3/propagation-delay-model.h"
#include "ns3/ssid.h"
#include "ns3/on-off-application.h"
#include "ns3/packet-sink.h"
#include "ns3/tcp-socket-factory.h"
#include "ns3/udp-socket-factory.h"
#include <iostream>
#include <fstream>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("MixedWifiExample");

int main (int argc, char *argv[])
{
  bool enableErpProtection = true;
  bool useShortPreamble = true;
  bool useShortSlotTime = true;
  std::string payloadSizeStr = "1472"; // Default UDP payload size
  uint32_t payloadSize = 1472;
  bool useUdp = true;

  CommandLine cmd;
  cmd.AddValue ("enableErpProtection", "Enable ERP protection mode", enableErpProtection);
  cmd.AddValue ("useShortPreamble", "Use short PPDU format", useShortPreamble);
  cmd.AddValue ("useShortSlotTime", "Use short slot time", useShortSlotTime);
  cmd.AddValue ("payloadSize", "Payload size (bytes)", payloadSizeStr);
  cmd.AddValue ("useUdp", "Use UDP traffic instead of TCP", useUdp);
  cmd.Parse (argc, argv);

  payloadSize = std::stoi(payloadSizeStr);

  Time::SetResolution (Time::NS);

  NodeContainer apNode;
  apNode.Create (1);

  NodeContainer bgNodes;
  bgNodes.Create (1);

  NodeContainer htNodes;
  htNodes.Create (1);

  WifiHelper wifiHelper;

  YansWifiPhyHelper wifiPhy =  YansWifiPhyHelper::Default ();
  wifiPhy.SetPcapDataLinkType (YansWifiPhyHelper::DLT_IEEE802_11_RADIO);

  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  wifiChannel.AddPropagationLoss ("ns3::FriisPropagationLossModel");
  wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
  wifiPhy.SetChannel (wifiChannel.Create ());

  WifiMacHelper wifiMac;
  Ssid ssid = Ssid ("ns3-mixed");

  // Setup AP (HT)
  WifiHelper wifiHelperAp;
  wifiHelperAp.SetStandard (WIFI_PHY_STANDARD_80211n);
  wifiMac.SetType ("ns3::ApWifiMac",
                   "Ssid", SsidValue (ssid));

  NetDeviceContainer apDevice = wifiHelperAp.Install (wifiPhy, wifiMac, apNode);

  // Setup b/g STA
  WifiHelper wifiHelperBg;
  wifiHelperBg.SetStandard (WIFI_PHY_STANDARD_80211g);
  wifiMac.SetType ("ns3::StaWifiMac",
                   "Ssid", SsidValue (ssid),
                   "ActiveProbing", BooleanValue (false));

  NetDeviceContainer bgDevice = wifiHelperBg.Install (wifiPhy, wifiMac, bgNodes);

  // Setup HT STA
  WifiHelper wifiHelperHt;
  wifiHelperHt.SetStandard (WIFI_PHY_STANDARD_80211n);
  wifiMac.SetType ("ns3::StaWifiMac",
                   "Ssid", SsidValue (ssid),
                   "ActiveProbing", BooleanValue (false));

  NetDeviceContainer htDevice = wifiHelperHt.Install (wifiPhy, wifiMac, htNodes);

  if (enableErpProtection)
  {
    Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/ErpProtection", BooleanValue (true));
  }

  if (useShortPreamble)
  {
    Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ShortPreambleSupported", BooleanValue (true));
  }

  if (useShortSlotTime)
  {
    Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Slot", TimeValue (Seconds (9e-6)));
  } else {
    Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Slot", TimeValue (Seconds (20e-6)));
  }


  // Mobility
  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (5.0),
                                 "DeltaY", DoubleValue (10.0),
                                 "GridWidth", UintegerValue (3),
                                 "LayoutType", StringValue ("RowFirst"));

  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (apNode);
  mobility.Install (bgNodes);
  mobility.Install (htNodes);

  // Internet stack
  InternetStackHelper internet;
  internet.Install (apNode);
  internet.Install (bgNodes);
  internet.Install (htNodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterface;
  apInterface = address.Assign (apDevice);
  Ipv4InterfaceContainer bgInterface;
  bgInterface = address.Assign (bgDevice);
  Ipv4InterfaceContainer htInterface;
  htInterface = address.Assign (htDevice);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // Traffic
  uint16_t port = 9;
  Address sinkLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), port));
  PacketSinkHelper packetSinkHelper ("ns3::UdpSocketFactory", sinkLocalAddress);

  ApplicationContainer sinkApp = packetSinkHelper.Install (htNodes.Get (0));
  sinkApp.Start (Seconds (1.0));
  sinkApp.Stop (Seconds (10.0));

  OnOffHelper onOffHelper (useUdp ? "ns3::UdpSocketFactory" : "ns3::TcpSocketFactory", Address (InetSocketAddress (htInterface.GetAddress (0), port)));
  onOffHelper.SetConstantRate (DataRate ("5Mb/s"));
  onOffHelper.SetAttribute ("PacketSize", UintegerValue (payloadSize));

  ApplicationContainer clientApp = onOffHelper.Install (bgNodes.Get (0));
  clientApp.Start (Seconds (2.0));
  clientApp.Stop (Seconds (10.0));


  // Flow monitor
  FlowMonitorHelper flowMonitorHelper;
  Ptr<FlowMonitor> flowMonitor = flowMonitorHelper.InstallAll ();

  Simulator::Stop (Seconds (11.0));

  Simulator::Run ();

  flowMonitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowMonitorHelper.GetClassifier ());
  FlowMonitor::FlowStatsContainer stats = flowMonitor->GetFlowStats ();

  double totalRxBytes = 0.0;
  double totalTxBytes = 0.0;
  Time totalDelay;

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      std::cout << "Flow ID: " << i->first  << " Src Addr " << t.sourceAddress << " Dst Addr " << t.destinationAddress << " Src Port " << t.sourcePort << " Dst Port " << t.destinationPort << "\n";
      std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
      std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
      std::cout << "  Lost Packets:   " << i->second.lostPackets << "\n";
      std::cout << "  Delay Sum:   " << i->second.delaySum << "\n";
      std::cout << "  Jitter Sum:   " << i->second.jitterSum << "\n";

      totalTxBytes += i->second.txBytes;
      totalRxBytes += i->second.rxBytes;
      totalDelay += i->second.delaySum;
    }

  double simulationTimeSeconds = 9.0; // Stop at 11s, start sending at 2s

  double throughput = (totalRxBytes * 8.0) / (simulationTimeSeconds * 1000000.0); // Mb/s

  std::cout << "---------------------------------------------------------" << std::endl;
  std::cout << "Simulation Results:" << std::endl;
  std::cout << "---------------------------------------------------------" << std::endl;
  std::cout << "ERP Protection: " << enableErpProtection << std::endl;
  std::cout << "Short Preamble: " << useShortPreamble << std::endl;
  std::cout << "Short Slot Time: " << useShortSlotTime << std::endl;
  std::cout << "Payload Size: " << payloadSize << " bytes" << std::endl;
  std::cout << "Transport Protocol: " << (useUdp ? "UDP" : "TCP") << std::endl;
  std::cout << "Throughput: " << throughput << " Mb/s" << std::endl;
  std::cout << "---------------------------------------------------------" << std::endl;

  flowMonitor->SerializeToXmlFile ("mixed-wifi-example.flowmon", true, true);

  Simulator::Destroy ();
  return 0;
}