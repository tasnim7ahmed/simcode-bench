#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"
#include <fstream>
#include <iostream>
#include <string>
#include <sstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiDistanceExperiment");

class DataCollector {
public:
  DataCollector (std::string experimentName, std::string format, std::string run)
    : m_experimentName (experimentName), m_format (format), m_run (run)
  {
  }

  void
  AddData (std::string key, std::string value)
  {
    m_data[key] = value;
  }

  void
  WriteData ()
  {
    if (m_format == "omnet")
      {
        WriteOmnetData ();
      }
    else if (m_format == "sqlite")
      {
        WriteSqliteData ();
      }
    else
      {
        std::cerr << "Unknown format: " << m_format << std::endl;
      }
  }

private:
  void
  WriteOmnetData ()
  {
    std::ofstream ofs;
    std::stringstream filename;
    filename << m_experimentName << "-" << m_run << ".sca";
    ofs.open (filename.str().c_str(), std::ios::out | std::ios::app);
    ofs << "run " << m_run << std::endl;
    for (const auto &pair : m_data)
      {
        ofs << "attr " << pair.first << " " << pair.second << std::endl;
      }
    ofs << "---" << std::endl;
    ofs.close ();
  }

  void
  WriteSqliteData ()
  {
    std::cerr << "Sqlite format not implemented yet." << std::endl;
  }

  std::string m_experimentName;
  std::string m_format;
  std::string m_run;
  std::map<std::string, std::string> m_data;
};

int
main (int argc, char *argv[])
{
  bool enablePcap = false;
  double distance = 10.0;
  std::string format = "omnet";
  std::string run = "0";

  CommandLine cmd;
  cmd.AddValue ("pcap", "Enable PCAP tracing", enablePcap);
  cmd.AddValue ("distance", "Distance between nodes", distance);
  cmd.AddValue ("format", "Output format (omnet, sqlite)", format);
  cmd.AddValue ("run", "Run identifier", run);
  cmd.Parse (argc, argv);

  NS_LOG_INFO ("Creating nodes.");
  NodeContainer nodes;
  nodes.Create (2);

  NS_LOG_INFO ("Setting up Wifi.");
  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Create ();
  wifiPhy.SetChannel (wifiChannel.Create ());

  WifiMacHelper wifiMac;
  wifiMac.SetType ("ns3::AdhocWifiMac");
  NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, nodes);

  NS_LOG_INFO ("Setting up Mobility.");
  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (distance),
                                 "DeltaY", DoubleValue (0.0),
                                 "GridWidth", UintegerValue (1),
                                 "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes);

  NS_LOG_INFO ("Setting up Internet Stack.");
  InternetStackHelper internet;
  internet.Install (nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign (devices);

  NS_LOG_INFO ("Setting up Applications.");
  uint16_t port = 9;
  UdpEchoServerHelper echoServer (port);

  ApplicationContainer serverApps = echoServer.Install (nodes.Get (1));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  UdpEchoClientHelper echoClient (interfaces.GetAddress (1), port);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (10));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps = echoClient.Install (nodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  if (enablePcap)
    {
      wifiPhy.EnablePcapAll ("wifi-distance");
    }

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  NS_LOG_INFO ("Running Simulation.");
  Simulator::Stop (Seconds (11.0));

  DataCollector dataCollector ("wifi-distance", format, run);
  dataCollector.AddData ("distance", std::to_string (distance));

  Simulator::Run ();

  monitor->CheckForLostPackets ();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      std::stringstream proto;
      proto << (uint16_t) t.protocol;
      NS_LOG_UNCOND("Flow ID: " << i->first << " Src Addr " << t.sourceAddress << " Dst Addr " << t.destinationAddress << " Src Port " << t.sourcePort << " Dst Port " << t.destinationPort);
      NS_LOG_UNCOND("  Tx Packets: " << i->second.txPackets);
      NS_LOG_UNCOND("  Rx Packets: " << i->second.rxPackets);
      NS_LOG_UNCOND("  Lost Packets: " << i->second.lostPackets);
      NS_LOG_UNCOND("  Throughput: " << i->second.txBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024  << " kbps");

      dataCollector.AddData ("flow-" + std::to_string(i->first) + "-txPackets", std::to_string(i->second.txPackets));
      dataCollector.AddData ("flow-" + std::to_string(i->first) + "-rxPackets", std::to_string(i->second.rxPackets));
      dataCollector.AddData ("flow-" + std::to_string(i->first) + "-lostPackets", std::to_string(i->second.lostPackets));
      dataCollector.AddData ("flow-" + std::to_string(i->first) + "-throughput", std::to_string(i->second.txBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024));
    }

  dataCollector.WriteData ();

  Simulator::Destroy ();
  NS_LOG_INFO ("Done.");

  return 0;
}