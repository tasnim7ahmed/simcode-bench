#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/udp-echo-helper.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"
#include "ns3/propagation-loss-model.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("AdhocWirelessNetwork");

int main (int argc, char *argv[])
{
  bool enableFlowMonitor = true;
  bool enableNetAnim = true;

  CommandLine cmd;
  cmd.AddValue ("EnableFlowMonitor", "Enable Flow Monitor", enableFlowMonitor);
  cmd.AddValue ("EnableNetAnim", "Enable NetAnim visualization", enableNetAnim);
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponent::SetLogLevel (LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create (2);

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Create ();
  wifiPhy.SetChannel (wifiChannel.Create ());

  NqosWifiMacHelper wifiMac = NqosWifiMacHelper::Default ();
  wifiMac.SetType ("ns3::AdhocWifiMac");

  NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, nodes);

  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::RandomBoxPositionAllocator",
                                 "X", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=10.0]"),
                                 "Y", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=10.0]"),
                                 "Z", StringValue ("ns3::ConstantRandomVariable[Constant=0.0]"));
  mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                             "Bounds", RectangleValue (Rectangle (0, 10, 0, 10)));
  mobility.Install (nodes);

  InternetStackHelper internet;
  internet.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("192.9.39.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  UdpEchoServerHelper echoServer (9);
  ApplicationContainer serverApps = echoServer.Install (nodes.Get (1));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  UdpEchoClientHelper echoClient (interfaces.GetAddress (1), 9);
  echoClient.SetAttribute ("MaxPackets", IntegerValue (100));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (0.01)));
  echoClient.SetAttribute ("PacketSize", IntegerValue (1024));
  ApplicationContainer clientApps = echoClient.Install (nodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

    UdpEchoServerHelper echoServer2 (10);
    ApplicationContainer serverApps2 = echoServer2.Install (nodes.Get (0));
    serverApps2.Start (Seconds (1.0));
    serverApps2.Stop (Seconds (10.0));

    UdpEchoClientHelper echoClient2 (interfaces.GetAddress (0), 10);
    echoClient2.SetAttribute ("MaxPackets", IntegerValue (100));
    echoClient2.SetAttribute ("Interval", TimeValue (Seconds (0.01)));
    echoClient2.SetAttribute ("PacketSize", IntegerValue (1024));
    ApplicationContainer clientApps2 = echoClient2.Install (nodes.Get (1));
    clientApps2.Start (Seconds (2.0));
    clientApps2.Stop (Seconds (10.0));

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor;
  if (enableFlowMonitor)
    {
      monitor = flowmon.InstallAll ();
    }

    if (enableNetAnim) {
        AnimationInterface anim("adhoc-wireless-network.xml");
        anim.SetConstantPosition(nodes.Get(0), 1.0, 1.0);
        anim.SetConstantPosition(nodes.Get(1), 5.0, 5.0);
    }

  Simulator::Stop (Seconds (11.0));
  Simulator::Run ();

  if (enableFlowMonitor)
    {
      monitor->CheckForLostPackets ();
      Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
      FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();
      for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
        {
          Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
          std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
          std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
          std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
          std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
          std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds () - i->second.timeFirstTxPacket.GetSeconds ()) / 1024  << " kbps\n";
        }
    }

  Simulator::Destroy ();
  return 0;
}