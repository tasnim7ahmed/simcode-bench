#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/gnuplot.h"
#include <iostream>
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiRateAdaptiveMinstrel");

int main (int argc, char *argv[])
{
  bool enableLogging = false;
  double simulationTime = 10.0;
  double distanceIncrement = 1.0;
  double timeIncrement = 0.5;
  std::string rateAdaptation = "MinstrelWifiManager";

  CommandLine cmd (__FILE__);
  cmd.AddValue ("enableLogging", "Enable logging of rate changes", enableLogging);
  cmd.AddValue ("simulationTime", "Simulation time (s)", simulationTime);
  cmd.AddValue ("distanceIncrement", "Distance increment (m)", distanceIncrement);
  cmd.AddValue ("timeIncrement", "Time increment (s)", timeIncrement);
  cmd.AddValue ("rateAdaptation", "Rate adaptation algorithm", rateAdaptation);
  cmd.Parse (argc, argv);

  LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);

  NodeContainer apNode;
  apNode.Create (1);
  NodeContainer staNode;
  staNode.Create (1);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211n);

  WifiMacHelper mac;
  Ssid ssid = Ssid ("ns-3-ssid");

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid));

  NetDeviceContainer apDevice = wifi.Install (phy, mac, apNode);

  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));

  if (rateAdaptation != "")
    {
      wifi.SetRemoteStationManager (rateAdaptation);
    }

  NetDeviceContainer staDevice = wifi.Install (phy, mac, staNode);

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

  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (staNode);

  InternetStackHelper internet;
  internet.Install (apNode);
  internet.Install (staNode);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterface = address.Assign (apDevice);
  Ipv4InterfaceContainer staInterface = address.Assign (staDevice);

  UdpServerHelper server (9);
  ApplicationContainer serverApps = server.Install (staNode);
  serverApps.Start (Seconds (0.0));
  serverApps.Stop (Seconds (simulationTime + 1));

  UdpClientHelper client (staInterface.GetAddress (0), 9);
  client.SetAttribute ("MaxPackets", UintegerValue (4294967295));
  client.SetAttribute ("Interval", TimeValue (Time ("0.000001s"))); // 1 us
  client.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps = client.Install (apNode);
  clientApps.Start (Seconds (1.0));
  clientApps.Stop (Seconds (simulationTime + 1));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  Gnuplot2dDataset dataset;
  dataset.SetTitle ("Throughput vs Distance");
  dataset.SetLegend ("Distance (m)", "Throughput (Mbps)");

  for (double distance = 5.0; distance <= 50.0; distance += distanceIncrement)
    {
      Ptr<MobilityModel> staMobility = staNode.Get (0)->GetObject<MobilityModel> ();
      staMobility->SetPosition (Vector (distance, 0.0, 0.0));

      Simulator::Run (Seconds (timeIncrement));

      monitor->CheckForLostPackets ();

      Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
      std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();

      double throughput = 0.0;
      for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
        {
          Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
          if (t.sourceAddress == apInterface.GetAddress (0) && t.destinationAddress == staInterface.GetAddress (0))
            {
              throughput = i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000000;
              break;
            }
        }

      dataset.Add (distance, throughput);
      monitor->ResetAll ();
    }

  monitor->SerializeToXmlFile ("wifi-rate-adaptive-minstrel.flowmon", true, true);

  Gnuplot plot ("wifi-rate-adaptive-minstrel.plt");
  plot.AddDataset (dataset);
  plot.GenerateOutput ("wifi-rate-adaptive-minstrel.png");

  Simulator::Destroy ();
  return 0;
}