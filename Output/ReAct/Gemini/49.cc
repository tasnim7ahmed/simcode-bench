#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/spectrum-module.h"
#include "ns3/command-line.h"
#include "ns3/uinteger.h"
#include "ns3/double.h"
#include "ns3/string.h"
#include "ns3/propagation-loss-model.h"
#include "ns3/propagation-delay-model.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/nqos-wifi-mac-helper.h"
#include "ns3/tcp-socket-base.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/error-model.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiInterferenceExample");

int main (int argc, char *argv[])
{
  bool verbose = false;
  bool tracing = false;
  double simulationTime = 10.0;
  double distance = 50.0;
  uint32_t mcsIndex = 7;
  uint32_t channelWidthIndex = 0;
  uint32_t guardIntervalIndex = 0;
  double waveformPower = 20.0;
  std::string wifiType = "ns3::YansWifiPhy";
  bool pcapTracing = false;
  double errorRate = 0.0;

  CommandLine cmd;
  cmd.AddValue ("verbose", "Tell application to log if set.", verbose);
  cmd.AddValue ("tracing", "Enable pcap tracing.", tracing);
  cmd.AddValue ("simulationTime", "Simulation time in seconds.", simulationTime);
  cmd.AddValue ("distance", "Distance between nodes.", distance);
  cmd.AddValue ("mcsIndex", "MCS index value.", mcsIndex);
  cmd.AddValue ("channelWidthIndex", "Channel width index (0-3).", channelWidthIndex);
  cmd.AddValue ("guardIntervalIndex", "Guard interval index (0 or 1).", guardIntervalIndex);
  cmd.AddValue ("waveformPower", "Interferer waveform power (dBm).", waveformPower);
  cmd.AddValue ("wifiType", "Type of Wi-Fi PHY (YansWifiPhy or SpectrumWifiPhy).", wifiType);
  cmd.AddValue ("pcap", "Enable PCAP tracing.", pcapTracing);
  cmd.AddValue ("errorRate", "Error rate for interferer.", errorRate);
  cmd.Parse (argc, argv);

  if (verbose)
    {
      LogComponentEnable ("WifiInterferenceExample", LOG_LEVEL_INFO);
      LogComponentEnable ("YansWifiPhy", LOG_LEVEL_ALL);
      LogComponentEnable ("SpectrumWifiPhy", LOG_LEVEL_ALL);
    }

  NodeContainer staNodes;
  staNodes.Create (2);

  NodeContainer apNode;
  apNode.Create (1);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211n);

  NqosWifiMacHelper mac = NqosWifiMacHelper::Default ();

  Ssid ssid = Ssid ("ns-3-ssid");
  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));

  NetDeviceContainer staDevices;
  staDevices = wifi.Install (phy, mac, staNodes);

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid));

  NetDeviceContainer apDevices;
  apDevices = wifi.Install (phy, mac, apNode);

  MobilityHelper mobility;

  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (distance),
                                 "DeltaY", DoubleValue (0.0),
                                 "GridWidth", UintegerValue (3),
                                 "LayoutType", StringValue ("RowFirst"));

  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");

  mobility.Install (staNodes);
  mobility.Install (apNode);

  InternetStackHelper stack;
  stack.Install (staNodes);
  stack.Install (apNode);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer staNodeInterfaces;
  staNodeInterfaces = address.Assign (staDevices);
  Ipv4InterfaceContainer apNodeInterfaces;
  apNodeInterfaces = address.Assign (apDevices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  uint16_t port = 50000;

  UdpClientHelper client (apNodeInterfaces.GetAddress (0), port);
  client.SetAttribute ("MaxPackets", UintegerValue (4294967295));
  client.SetAttribute ("Interval", TimeValue (Seconds (0.00002))); // send every 20 usec
  client.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps = client.Install (staNodes.Get (0));
  clientApps.Start (Seconds (1.0));
  clientApps.Stop (Seconds (simulationTime - 1));

  UdpServerHelper echoServer (port);
  ApplicationContainer serverApps = echoServer.Install (apNode.Get (0));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (simulationTime - 1));

  NodeContainer interfererNode;
  interfererNode.Create (1);
  MobilityHelper mobilityInterferer;
  mobilityInterferer.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobilityInterferer.Install (interfererNode);
  interfererNode.Get(0)->GetObject<MobilityModel>()->SetPosition(Vector(distance/2.0, 0.0, 0.0));

  OnOffHelper onoff ("ns3::UdpSocketFactory", Address (InetSocketAddress (apNodeInterfaces.GetAddress (0), port)));
  onoff.SetConstantRate (DataRate ("100Mbps"));
  ApplicationContainer onOffApps = onoff.Install (interfererNode.Get (0));
  onOffApps.Start (Seconds (1.0));
  onOffApps.Stop (Seconds (simulationTime - 1));

  Ptr<UniformRandomVariable> uv = CreateObject<UniformRandomVariable> ();
  uv->SetStream (50);
  Ptr<RateErrorModel> em = CreateObject<RateErrorModel> ();
  em->SetAttribute ("ErrorRate", DoubleValue (errorRate));
  em->SetRandomVariable (uv);
  onoff.SetAttribute("PacketSize", UintegerValue(1024));

  Ptr<OutputStreamWrapper> stream = Create<OutputStreamWrapper> ("wifi-interference.txt", std::ios::out);
  phy.EnableAsciiAll (stream);

  if (pcapTracing)
    {
      phy.EnablePcap ("wifi-interference", apDevices);
      phy.EnablePcap ("wifi-interference", staDevices.Get(0));
      phy.EnablePcap ("wifi-interference", staDevices.Get(1));
    }

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  Simulator::Stop (Seconds (simulationTime));

  Simulator::Run ();

  monitor->CheckForLostPackets ();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
      std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
      std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
      std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024 / 1024  << " Mbps\n";
    }

  monitor->SerializeToXmlFile("wifi-interference.flowmon", true, true);

  Simulator::Destroy ();

  return 0;
}