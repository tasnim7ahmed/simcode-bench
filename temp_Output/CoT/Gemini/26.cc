#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/mobility-module.h"
#include "ns3/matrix-propagation-loss-model.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  bool enableRtsCts = false;

  CommandLine cmd;
  cmd.AddValue ("EnableRtsCts", "Enable RTS/CTS", enableRtsCts);
  cmd.Parse (argc, argv);

  Config::SetDefault ("ns3::WifiMac::RtsCtsThreshold", StringValue (enableRtsCts ? "100" : "2200"));
  Config::SetDefault ("ns3::WifiMac::FragmentThreshold", StringValue ("2200"));

  NodeContainer nodes;
  nodes.Create (3);

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  MatrixPropagationLossModel matrixLossModel;
  matrixLossModel.SetLoss (0, 1, 50);
  matrixLossModel.SetLoss (1, 0, 50);
  matrixLossModel.SetLoss (1, 2, 50);
  matrixLossModel.SetLoss (2, 1, 50);
  matrixLossModel.SetLoss (0, 2, 200);
  matrixLossModel.SetLoss (2, 0, 200);

  phy.SetPropagationLossModel (matrixLossModel);

  NqosWifiMacHelper mac = NqosWifiMacHelper::Default ();
  mac.SetType ("ns3::AdhocWifiMac");

  NetDeviceContainer devices = wifi.Install (phy, mac, nodes);

  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (10.0),
                                 "DeltaY", DoubleValue (10.0),
                                 "GridWidth", UintegerValue (3),
                                 "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes);

  InternetStackHelper internet;
  internet.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  OnOffHelper onoff1 ("ns3::UdpSocketFactory", Address (InetSocketAddress (interfaces.GetAddress (1), 9)));
  onoff1.SetConstantRate (DataRate ("1Mb/s"));
  ApplicationContainer app1 = onoff1.Install (nodes.Get (0));
  app1.Start (Seconds (1.0));
  app1.Stop (Seconds (10.0));

  OnOffHelper onoff2 ("ns3::UdpSocketFactory", Address (InetSocketAddress (interfaces.GetAddress (1), 9)));
  onoff2.SetConstantRate (DataRate ("1Mb/s"));
  ApplicationContainer app2 = onoff2.Install (nodes.Get (2));
  app2.Start (Seconds (1.5));
  app2.Stop (Seconds (10.0));

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  Simulator::Stop (Seconds (10.0));

  Simulator::Run ();

  monitor->CheckForLostPackets ();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      std::cout << "Flow " << i->first  << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
      std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
      std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
      std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024  << " kbps\n";
      std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
    }

  Simulator::Destroy ();

  return 0;
}