#include <iostream>
#include <fstream>
#include <string>
#include <sstream>

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/command-line.h"
#include "ns3/uinteger.h"
#include "ns3/double.h"
#include "ns3/string.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("RateAdaptiveWifi");

int main (int argc, char *argv[])
{
  bool enableLogging = false;
  double distanceStep = 1.0;
  double timeStep = 1.0;
  double simulationTime = 10.0;
  std::string phyMode ("HtMcs7");

  CommandLine cmd;
  cmd.AddValue ("enableLogging", "Enable rate change logging", enableLogging);
  cmd.AddValue ("distanceStep", "Distance step for STA movement (m)", distanceStep);
  cmd.AddValue ("timeStep", "Time step for STA movement (s)", timeStep);
  cmd.AddValue ("simulationTime", "Total simulation time (s)", simulationTime);
  cmd.AddValue ("phyMode", "Wifi Phy mode", phyMode);
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);

  NodeContainer apNode;
  apNode.Create (1);
  NodeContainer staNode;
  staNode.Create (1);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211n);

  WifiMacHelper mac;
  Ssid ssid = Ssid ("ns-3-ssid");

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid));

  NetDeviceContainer apDevice = wifi.Install (phy, mac, apNode);

  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));
  wifi.SetRemoteStationManager ("ns3::MinstrelWifiManager");
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

  Ptr<MobilityModel> staMobility = staNode.Get (0)->GetObject<MobilityModel> ();
  staMobility->SetPosition (Vector (5.0, 0.0, 0.0));

  InternetStackHelper internet;
  internet.Install (apNode);
  internet.Install (staNode);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterface = address.Assign (apDevice);
  Ipv4InterfaceContainer staInterface = address.Assign (staDevice);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  uint16_t port = 9;
  OnOffHelper onoff ("ns3::UdpSocketFactory",
                     Address (InetSocketAddress (staInterface.GetAddress (0), port)));
  onoff.SetConstantRate (DataRate ("400Mbps"));
  ApplicationContainer app = onoff.Install (apNode.Get (0));
  app.Start (Seconds (1.0));
  app.Stop (Seconds (simulationTime));

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  if (enableLogging)
  {
    Config::Connect ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/Minstrel/TxRate",
                      MakeCallback (&
                                    [] (std::string path, WifiMode mode)
                                    {
                                      std::cout << Simulator::Now ().GetSeconds () << " " << path << " " << mode << std::endl;
                                    }));
  }

  Simulator::Schedule (Seconds (0.0), [&] () {
    phy.SetPcapDataLinkType (WifiPhyHelper::DLT_IEEE802_11_RADIO);
    phy.EnablePcap ("rate_adaptive", apDevice.Get (0));
    phy.EnablePcap ("rate_adaptive", staDevice.Get (0));
  });

  for (double t = timeStep; t <= simulationTime; t += timeStep)
  {
    Simulator::Schedule (Seconds (t), [staMobility, distanceStep] () {
      Vector pos = staMobility->GetPosition ();
      pos.x += distanceStep;
      staMobility->SetPosition (pos);
    });
  }

  Simulator::Stop (Seconds (simulationTime + 1));

  Simulator::Run ();

  monitor->CheckForLostPackets ();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();

  std::ofstream out ("rate_adaptive.dat");
  out << "# Time (s) Distance (m) Throughput (Mbps)" << std::endl;

  for (double t = 1.0; t <= simulationTime; t += timeStep)
  {
    double distance = 5.0 + (t / timeStep) * distanceStep;
    double throughput = 0.0;

    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      if (t.sourceAddress == apInterface.GetAddress (0) && t.destinationAddress == staInterface.GetAddress (0) && t.destinationPort == port)
      {
        throughput = i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds () - i->second.timeFirstTxPacket.GetSeconds ()) / 1000000;
        break;
      }
    }
    out << t << " " << distance << " " << throughput << std::endl;
  }
  out.close ();

  Simulator::Destroy ();

  std::ofstream gnuplotFile ("rate_adaptive.plt");
  gnuplotFile << "set terminal png size 1024,768" << std::endl;
  gnuplotFile << "set output 'rate_adaptive.png'" << std::endl;
  gnuplotFile << "set title 'Throughput vs. Distance'" << std::endl;
  gnuplotFile << "set xlabel 'Distance (m)'" << std::endl;
  gnuplotFile << "set ylabel 'Throughput (Mbps)'" << std::endl;
  gnuplotFile << "plot 'rate_adaptive.dat' using 2:3 with lines title 'Throughput'" << std::endl;
  gnuplotFile.close ();

  return 0;
}