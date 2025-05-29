#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-helper.h"
#include <fstream>
#include <iostream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("AdhocWiFiSimulation");

int main (int argc, char *argv[])
{
  uint32_t numNodes = 5;
  double simTime = 20.0;
  std::string outputFileName = "adhoc-wifi-results.txt";

  CommandLine cmd;
  cmd.AddValue ("outputFile", "Output file for metrics", outputFileName);
  cmd.Parse (argc, argv);

  NodeContainer nodes;
  nodes.Create (numNodes);

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  wifiPhy.SetChannel (wifiChannel.Create ());

  WifiMacHelper wifiMac;
  wifiMac.SetType ("ns3::AdhocWifiMac");

  NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, nodes);

  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::RandomRectanglePositionAllocator",
                                "X", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                "Y", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"));
  mobility.SetMobilityModel ("ns3::RandomWaypointMobilityModel",
                             "Speed", StringValue ("ns3::UniformRandomVariable[Min=1.0|Max=5.0]"),
                             "Pause", StringValue ("ns3::ConstantRandomVariable[Constant=0.5]"),
                             "PositionAllocator", PointerValue (mobility.GetPositionAllocator ()));
  mobility.Install (nodes);

  InternetStackHelper internet;
  internet.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  uint16_t port = 4000;
  uint32_t payloadSize = 1024;
  double interval = 0.05;

  // UDP Server (node 4)
  UdpServerHelper server (port);
  ApplicationContainer serverApp = server.Install (nodes.Get (4));
  serverApp.Start (Seconds (1.0));
  serverApp.Stop (Seconds (simTime));

  // UDP Client (node 0 -> node 4)
  UdpClientHelper client (interfaces.GetAddress (4), port);
  client.SetAttribute ("MaxPackets", UintegerValue (uint32_t ((simTime-2.0)/interval)));
  client.SetAttribute ("Interval", TimeValue (Seconds (interval)));
  client.SetAttribute ("PacketSize", UintegerValue (payloadSize));
  ApplicationContainer clientApp = client.Install (nodes.Get (0));
  clientApp.Start (Seconds (2.0));
  clientApp.Stop (Seconds (simTime));

  // FlowMonitor
  FlowMonitorHelper flowmonHelper;
  Ptr<FlowMonitor> flowmon = flowmonHelper.InstallAll ();

  Simulator::Stop (Seconds (simTime));
  Simulator::Run ();

  flowmon->CheckForLostPackets ();
  std::map<FlowId, FlowMonitor::FlowStats> stats = flowmon->GetFlowStats ();

  std::ofstream outFile;
  outFile.open (outputFileName, std::ios::out | std::ios::trunc);

  outFile << "FlowID,SrcAddr,DstAddr,TxPackets,RxPackets,LostPackets,Throughput_bps\n";
  for (auto const& flow : stats)
    {
      Ipv4FlowClassifier::FiveTuple t = flowmonHelper.GetClassifier ()->FindFlow (flow.first);
      double throughput = (flow.second.rxBytes * 8.0) / (simTime - 2.0); // ignore first 2s
      outFile << flow.first << ","
              << t.sourceAddress << ","
              << t.destinationAddress << ","
              << flow.second.txPackets << ","
              << flow.second.rxPackets << ","
              << (flow.second.txPackets - flow.second.rxPackets) << ","
              << throughput
              << "\n";
    }

  outFile.close ();

  Simulator::Destroy ();
  return 0;
}