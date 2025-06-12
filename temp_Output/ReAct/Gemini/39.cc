#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/gnuplot.h"
#include "ns3/command-line.h"
#include <iostream>
#include <fstream>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiRateAdaptiveMinstrel");

int main (int argc, char *argv[])
{
  bool enableLogging = false;
  double simulationTime = 10.0;
  std::string rate = "HtMcs7";
  double distanceStep = 10.0;
  double interval = 1.0;

  CommandLine cmd;
  cmd.AddValue ("enableLogging", "Enable logging", enableLogging);
  cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue ("rate", "Wifi transmit rate", rate);
  cmd.AddValue ("distanceStep", "Distance step in meters", distanceStep);
  cmd.AddValue ("interval", "Time interval between steps in seconds", interval);
  cmd.Parse (argc, argv);

  LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create (2);

  NodeContainer apNode = NodeContainer (nodes.Get (0));
  NodeContainer staNode = NodeContainer (nodes.Get (1));

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiHelper wifi;
  wifi.SetStandard (WIFI_80211n);

  WifiMacHelper mac;

  Ssid ssid = Ssid ("ns3-wifi");

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid));

  NetDeviceContainer apDevice = wifi.Install (phy, mac, apNode);

  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));

  NetDeviceContainer staDevice = wifi.Install (phy, mac, staNode);

  // Install rate adaptation protocol for the STA
  std::string minstrelName = "ns3::MinstrelHtWifiManager";
  Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/ এডুকেট", StringValue (minstrelName));

  // Set a fixed rate for the AP
  std::string fixedRate = "ns3::ConstantRateWifiManager";
    AttributeValue rateValue;
    rateValue.SetFromString (rate);
  Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/এডুকেট", StringValue (fixedRate));
  Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/এডুকেট", rateValue);


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

  Ptr<ConstantPositionMobilityModel> staMobility = CreateObject<ConstantPositionMobilityModel> ();
  staNode.Get(0)->AggregateObject (staMobility);
  staMobility->SetPosition (Vector (5.0, 0.0, 0.0));

  InternetStackHelper internet;
  internet.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (apDevice.Get (0), staDevice.Get (0));

  UdpServerHelper server (9);
  ApplicationContainer serverApps = server.Install (staNode);
  serverApps.Start (Seconds (0.0));
  serverApps.Stop (Seconds (simulationTime + 1));

  UdpClientHelper client (interfaces.GetAddress (1), 9);
  client.SetAttribute ("MaxPackets", UintegerValue (4294967295));
  client.SetAttribute ("Interval", TimeValue (Time ("ns3::Time", Seconds (0.000001))));
  client.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps = client.Install (apNode);
  clientApps.Start (Seconds (1.0));
  clientApps.Stop (Seconds (simulationTime + 1));

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  // Configure trace sinks
  if (enableLogging)
    {
      phy.EnablePcap ("wifi-rate-adaptive", apDevice.Get (0));
      phy.EnablePcap ("wifi-rate-adaptive", staDevice.Get (0));
    }

  // Schedule mobility changes
  Simulator::Schedule (Seconds (0.0), [&, staMobility]() {
    NS_LOG_UNCOND ("Distance: " << staMobility->GetPosition ().x);
  });

  double currentTime = interval;
  double currentDistance = 5.0;
  while (currentTime <= simulationTime)
    {
      currentDistance += distanceStep;
      Simulator::Schedule (Seconds (currentTime), [&, staMobility, currentDistance]() {
          staMobility->SetPosition (Vector (currentDistance, 0.0, 0.0));
          NS_LOG_UNCOND ("Distance: " << staMobility->GetPosition ().x);
        });
      currentTime += interval;
    }

  Simulator::Stop (Seconds (simulationTime + 2));

  // Run the simulation
  Simulator::Run ();

  monitor->CheckForLostPackets ();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();

  std::map<double, double> throughputData;

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      if (t.destinationAddress == interfaces.GetAddress (1))
        {
          double totalThroughput = (i->second.rxBytes * 8.0) / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds())/1000000.0;
           throughputData[staMobility->GetPosition().x] = totalThroughput;
           std::cout << "Flow " << i->first  << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
           std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
           std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
           std::cout << "  Throughput: " << totalThroughput << " Mbps\n";
        }
    }

  monitor->SerializeToXmlFile("wifi-rate-adaptive.flowmon", true, true);

  // Create Gnuplot data file
  std::ofstream out ("wifi-rate-adaptive.dat");
  if (!out.is_open ())
    {
      std::cerr << "Could not open wifi-rate-adaptive.dat for writing" << std::endl;
      return 1;
    }

  for (const auto& pair : throughputData) {
      out << pair.first << " " << pair.second << std::endl;
  }
  out.close();

  // Create Gnuplot script file
  std::ofstream gnuplotFile ("wifi-rate-adaptive.plt");
  if (!gnuplotFile.is_open ())
    {
      std::cerr << "Could not open wifi-rate-adaptive.plt for writing" << std::endl;
      return 1;
    }

    gnuplotFile << "set terminal png size 1024,768" << std::endl;
    gnuplotFile << "set output 'wifi-rate-adaptive.png'" << std::endl;
    gnuplotFile << "set title 'Throughput vs. Distance'" << std::endl;
    gnuplotFile << "set xlabel 'Distance (m)'" << std::endl;
    gnuplotFile << "set ylabel 'Throughput (Mbps)'" << std::endl;
    gnuplotFile << "plot 'wifi-rate-adaptive.dat' with lines title 'Throughput'" << std::endl;
    gnuplotFile.close();

  Simulator::Destroy ();

  return 0;
}