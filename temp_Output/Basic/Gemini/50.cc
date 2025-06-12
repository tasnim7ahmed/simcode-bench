#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include <iostream>
#include <fstream>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiMultiMCS");

double StringToDouble(const std::string& str) {
    std::istringstream i(str);
    double x;
    if (!(i >> x)) {
        return 0.0;
    }
    return x;
}

int main (int argc, char *argv[])
{
  bool verbose = false;
  uint32_t nPackets = 1000;
  double interval = 0.0001;
  double simulationTime = 10;
  std::string dataRate = "6Mbps";
  std::string phyMode ("OfdmRate6Mbps");
  double distance = 10;
  bool pcapTracing = false;
  std::string wifiStandard = "802.11a";
  std::string errorModelType = "NistErrorRateModel";
  int mcsIndex = 0;
  int channelWidth = 20;
  std::string guardInterval = "SHORT";

  CommandLine cmd;
  cmd.AddValue ("verbose", "Tell application to log if set.", verbose);
  cmd.AddValue ("nPackets", "Number of packets generated", nPackets);
  cmd.AddValue ("interval", "Interval between packets in seconds", interval);
  cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue ("dataRate", "Data rate for UDP traffic", dataRate);
  cmd.AddValue ("phyMode", "Wifi phy mode", phyMode);
  cmd.AddValue ("distance", "Distance between nodes", distance);
  cmd.AddValue ("pcap", "Enable pcap tracing", pcapTracing);
  cmd.AddValue ("wifiStandard", "Wifi standard (e.g., 802.11a, 802.11n, 802.11ac)", wifiStandard);
  cmd.AddValue ("errorModelType", "Error model type", errorModelType);
  cmd.AddValue ("mcsIndex", "MCS index (0-11 for 802.11n, 0-9 for 802.11ac)", mcsIndex);
  cmd.AddValue ("channelWidth", "Channel width in MHz (e.g., 20, 40, 80)", channelWidth);
  cmd.AddValue ("guardInterval", "Guard interval (SHORT, LONG)", guardInterval);
  cmd.Parse (argc, argv);

  if (verbose)
    {
      LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
      LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);
    }

  NodeContainer staNodes;
  staNodes.Create (1);

  NodeContainer apNodes;
  apNodes.Create (1);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());
  phy.SetErrorRateModel(errorModelType);

  WifiHelper wifi;
  wifi.SetStandard (wifiStandard);

  WifiMacHelper mac;
  Ssid ssid = Ssid ("ns-3-ssid");

  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));

  NetDeviceContainer staDevices;
  staDevices = wifi.Install (phy, mac, staNodes);

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid),
               "BeaconInterval", TimeValue (Seconds (0.1)));

  NetDeviceContainer apDevices;
  apDevices = wifi.Install (phy, mac, apNodes);

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
  mobility.Install (apNodes);

  InternetStackHelper stack;
  stack.Install (apNodes);
  stack.Install (staNodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterfaces = address.Assign (apDevices);
  Ipv4InterfaceContainer staInterfaces = address.Assign (staDevices);

  UdpServerHelper echoServer (9);
  ApplicationContainer serverApps = echoServer.Install (apNodes.Get (0));
  serverApps.Start (Seconds (0.0));
  serverApps.Stop (Seconds (simulationTime + 1));

  UdpClientHelper echoClient (apInterfaces.GetAddress (0), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (nPackets));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (interval)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps = echoClient.Install (staNodes.Get (0));
  clientApps.Start (Seconds (1.0));
  clientApps.Stop (Seconds (simulationTime + 1));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  Simulator::Stop (Seconds (simulationTime + 1));

  if (pcapTracing)
    {
      phy.EnablePcap ("wifi-multi-mcs", apDevices.Get (0));
    }

  Simulator::Run ();

  monitor->CheckForLostPackets ();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
	  Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);

      std::cout << "Flow ID: " << i->first << " Src Addr " << t.sourceAddress << " Dst Addr " << t.destinationAddress << "\n";
      std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
      std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
      std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024/1024  << " Mbps\n";
      std::cout << "  MCS Index: " << mcsIndex << "\n";
      std::cout << "  Channel Width: " << channelWidth << " MHz\n";
      std::cout << "  Guard Interval: " << guardInterval << "\n";
    }

  Simulator::Destroy ();

  return 0;
}