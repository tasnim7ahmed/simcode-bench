#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("AdhocGridNetwork");

int main (int argc, char *argv[])
{
  bool enableFlowMonitor = true;
  bool enableNetAnim = true;

  CommandLine cmd;
  cmd.AddValue ("enableFlowMonitor", "Enable Flow Monitor", enableFlowMonitor);
  cmd.AddValue ("enableNetAnim", "Enable NetAnim", enableNetAnim);
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponent::SetFilter ("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponent::SetFilter ("UdpEchoServerApplication", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create (6);

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

  YansWifiPhyHelper wifiPhy =  YansWifiPhyHelper::Default ();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  wifiPhy.SetChannel (wifiChannel.Create ());

  NqosWifiMacHelper wifiMac = NqosWifiMacHelper::Default ();
  wifiMac.SetType ("ns3::AdhocWifiMac");

  NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, nodes);

  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (5.0),
                                 "DeltaY", DoubleValue (5.0),
                                 "GridWidth", UintegerValue (3),
                                 "LayoutType", StringValue ("RowFirst"));

  mobility.SetMobilityModel ("ns3::RandomBoxMobilityModel",
                             "Bounds", BoxValue (Box (0, 0, 0, 20, 20, 10)));
  mobility.Install (nodes);

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("192.9.39.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  UdpEchoServerHelper echoServer (9);
  ApplicationContainer serverApps = echoServer.Install (nodes);
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (20.0));

  UdpEchoClientHelper echoClient (interfaces.GetAddress (1), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (20));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (0.5)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

    ApplicationContainer clientApps;
  for (uint32_t i = 0; i < nodes.GetN (); ++i)
  {
        uint32_t destNode = (i + 1) % nodes.GetN ();
        UdpEchoClientHelper clientHelper (interfaces.GetAddress (destNode), 9);
        clientHelper.SetAttribute ("MaxPackets", UintegerValue (20));
        clientHelper.SetAttribute ("Interval", TimeValue (Seconds (0.5)));
        clientHelper.SetAttribute ("PacketSize", UintegerValue (1024));
        clientApps.Add (clientHelper.Install (nodes.Get (i)));
  }

  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (20.0));

  if (enableFlowMonitor)
    {
      FlowMonitorHelper flowmon;
      Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

      Simulator::Stop (Seconds (20.0));
      Simulator::Run ();

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
          std::cout << "  Packet Delivery Ratio: " << (double) i->second.rxPackets / (double) i->second.txPackets << "\n";
          std::cout << "  Delay Sum: " << i->second.delaySum << "\n";
          std::cout << "  Mean Delay: " << i->second.delaySum / i->second.rxPackets << "\n";
          std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024 / 1024  << " Mbps\n";
        }

      monitor->SerializeToXmlFile("AdhocGridNetwork.flowmon", true, true);
    }
  else
    {
      Simulator::Stop (Seconds (20.0));
      Simulator::Run ();
    }

  if(enableNetAnim){
      AnimationInterface anim("AdhocGridNetwork.xml");
      anim.EnablePacketMetadata ();
      anim.SetMaxPktsPerTraceFile(500000);
  }
  Simulator::Destroy ();
  return 0;
}