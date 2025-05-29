#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/aodv-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/udp-client-server-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("UAVNetwork");

int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponent::SetFilter ( "UdpClient", Level::LOG_LEVEL_INFO );

  NodeContainer uavNodes;
  uavNodes.Create (3);

  NodeContainer gcsNode;
  gcsNode.Create (1);

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiMacHelper mac;
  mac.SetType (WifiMacHelper::ADHOC,
               "Ssid", Ssid ("ns-3-adhoc"));

  NetDeviceContainer uavNetDevices = wifi.Install (phy, mac, uavNodes);
  NetDeviceContainer gcsNetDevices = wifi.Install (phy, mac, gcsNode);

  AodvHelper aodv;
  InternetStackHelper internet;
  internet.SetRoutingHelper (aodv);
  internet.Install (uavNodes);
  internet.Install (gcsNode);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer uavInterfaces = ipv4.Assign (uavNetDevices);
  Ipv4InterfaceContainer gcsInterfaces = ipv4.Assign (gcsNetDevices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::RandomBoxPositionAllocator",
                                 "X", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                 "Y", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                 "Z", StringValue ("ns3::UniformRandomVariable[Min=10.0|Max=20.0]"));

  mobility.SetMobilityModel ("ns3::RandomWalk3dMobilityModel",
                             "Mode", StringValue ("Time"),
                             "Time", StringValue ("1s"),
                             "Speed", StringValue ("ns3::ConstantRandomVariable[Constant=10]"));
  mobility.Install (uavNodes);

  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (gcsNode);

  UdpClientServerHelper echoClientServer (9);
  echoClientServer.SetAttribute ("MaxPackets", UintegerValue (1000));
  echoClientServer.SetAttribute ("Interval", TimeValue (Seconds (0.1)));
  echoClientServer.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps;
  Address serverAddress (InetSocketAddress (gcsInterfaces.GetAddress (0), 9));

  for (uint32_t i = 0; i < uavNodes.GetN (); ++i) {
      echoClientServer.SetAttribute("RemoteAddress", AddressValue(serverAddress));
      clientApps.Add (echoClientServer.Install (uavNodes.Get (i)));
  }

  ApplicationContainer serverApps = echoClientServer.Install (gcsNode.Get (0));

  clientApps.Start (Seconds (1.0));
  clientApps.Stop (Seconds (59.0));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (59.0));

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  Simulator::Stop (Seconds (60));
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
      std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024  << " kbps\n";
    }

  Simulator::Destroy ();
  return 0;
}