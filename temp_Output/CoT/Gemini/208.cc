#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/netanim-module.h"
#include "ns3/olsr-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiHandover");

int main (int argc, char *argv[])
{
  bool tracing = true;
  double simulationTime = 10.0;
  double distance = 100.0;

  CommandLine cmd;
  cmd.AddValue ("tracing", "Enable or disable tracing", tracing);
  cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue ("distance", "The distance between APs", distance);
  cmd.Parse (argc, argv);

  NodeContainer apNodes;
  apNodes.Create (2);

  NodeContainer staNodes;
  staNodes.Create (6);

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiMacHelper mac;
  Ssid ssid1 = Ssid ("ns-3-ssid-1");
  Ssid ssid2 = Ssid ("ns-3-ssid-2");

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid1),
               "BeaconInterval", TimeValue (Seconds (0.1)),
               "QosSupported", BooleanValue (true),
               "EnableBeaconJitter", BooleanValue (false));

  NetDeviceContainer apDevices1 = wifi.Install (phy, mac, apNodes.Get (0));

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid2),
               "BeaconInterval", TimeValue (Seconds (0.1)),
               "QosSupported", BooleanValue (true),
               "EnableBeaconJitter", BooleanValue (false));

  NetDeviceContainer apDevices2 = wifi.Install (phy, mac, apNodes.Get (1));

  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid1),
               "ActiveProbing", BooleanValue (false));

  NetDeviceContainer staDevices = wifi.Install (phy, mac, staNodes);

  MobilityHelper mobility;

  mobility.SetPositionAllocator ("ns3::RandomBoxPositionAllocator",
                                 "X", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                 "Y", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                 "Z", StringValue ("ns3::ConstantRandomVariable[Constant=0.0]"));

  mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                             "Mode", StringValue ("Time"),
                             "Time", StringValue ("2s"),
                             "Speed", StringValue ("ns3::ConstantRandomVariable[Constant=1.0]"),
                             "Bounds", StringValue ("0 100 0 100"));
  mobility.Install (staNodes);

  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (apNodes);

  Ptr<ConstantPositionMobilityModel> ap0Mobility = apNodes.Get (0)->GetObject<ConstantPositionMobilityModel> ();
  ap0Mobility->SetPosition (Vector (0.0, 50.0, 0.0));

  Ptr<ConstantPositionMobilityModel> ap1Mobility = apNodes.Get (1)->GetObject<ConstantPositionMobilityModel> ();
  ap1Mobility->SetPosition (Vector (distance, 50.0, 0.0));

  InternetStackHelper internet;
  internet.Install (apNodes);
  internet.Install (staNodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterfaces1 = address.Assign (apDevices1);

  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterfaces2 = address.Assign (apDevices2);

  address.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer staInterfaces = address.Assign (staDevices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  UdpEchoServerHelper echoServer (9);
  ApplicationContainer serverApps = echoServer.Install (staNodes.Get (0));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (simulationTime));

  UdpEchoClientHelper echoClient (staInterfaces.GetAddress (0), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (100000));
  echoClient.SetAttribute ("Interval", TimeValue (MilliSeconds (1)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientApps = echoClient.Install (staNodes.Get (1));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (simulationTime));

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  Simulator::Stop (Seconds (simulationTime));

  AnimationInterface anim ("wifi-handover.xml");
  anim.SetConstantPosition (apNodes.Get (0), 0, 50);
  anim.SetConstantPosition (apNodes.Get (1), distance, 50);

  if (tracing == true)
    {
      phy.EnablePcapAll ("wifi-handover");
    }

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
      std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds () - i->second.timeFirstTxPacket.GetSeconds ()) / 1024  << " kbps\n";
    }

  monitor->SerializeToXmlFile("wifi-handover.flowmon", true, true);

  Simulator::Destroy ();
  return 0;
}