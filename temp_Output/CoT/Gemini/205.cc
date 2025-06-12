#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("VehicularNetwork");

int main (int argc, char *argv[])
{
  bool enableNetAnim = true;
  double simulationTime = 10.0;

  CommandLine cmd;
  cmd.AddValue ("enableNetAnim", "Enable NetAnim visualization", enableNetAnim);
  cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.Parse (argc, argv);

  NodeContainer nodes;
  nodes.Create (4);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211a);
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                "DataMode", StringValue ("OfdmRate6Mbps"),
                                "RtsCtsThreshold", UintegerValue (2000));

  NqosWifiMacHelper mac = NqosWifiMacHelper::Default ();
  mac.SetType ("ns3::AdhocWifiMac");

  NetDeviceContainer devices = wifi.Install (phy, mac, nodes);

  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (50.0),
                                 "DeltaY", DoubleValue (0.0),
                                 "GridWidth", UintegerValue (4),
                                 "LayoutType", StringValue ("RowFirst"));

  mobility.SetMobilityModel ("ns3::ConstantVelocityMobilityModel");
  mobility.Install (nodes);

  Ptr<ConstantVelocityMobilityModel> cvmm;
  cvmm = nodes.Get(0)->GetObject<ConstantVelocityMobilityModel>();
  cvmm->SetVelocity(Vector3D(10,0,0));
  cvmm = nodes.Get(1)->GetObject<ConstantVelocityMobilityModel>();
  cvmm->SetVelocity(Vector3D(10,0,0));
  cvmm = nodes.Get(2)->GetObject<ConstantVelocityMobilityModel>();
  cvmm->SetVelocity(Vector3D(10,0,0));
  cvmm = nodes.Get(3)->GetObject<ConstantVelocityMobilityModel>();
  cvmm->SetVelocity(Vector3D(10,0,0));
  
  InternetStackHelper internet;
  internet.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  UdpEchoServerHelper echoServer (9);
  ApplicationContainer serverApps = echoServer.Install (nodes.Get (3));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (simulationTime - 1));

  UdpEchoClientHelper echoClient (interfaces.GetAddress (3), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (100));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (0.1)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps = echoClient.Install (nodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (simulationTime - 2));

  clientApps = echoClient.Install (nodes.Get (1));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (simulationTime - 2));

  clientApps = echoClient.Install (nodes.Get (2));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (simulationTime - 2));
  
  FlowMonitorHelper flowMonitor;
  Ptr<FlowMonitor> monitor = flowMonitor.InstallAll ();

  if (enableNetAnim)
    {
      AnimationInterface anim ("vehicular-network.xml");
      anim.SetConstantPosition (nodes.Get (0), 0.0, 10.0);
      anim.SetConstantPosition (nodes.Get (1), 50.0, 10.0);
      anim.SetConstantPosition (nodes.Get (2), 100.0, 10.0);
      anim.SetConstantPosition (nodes.Get (3), 150.0, 10.0);
    }

  Simulator::Stop (Seconds (simulationTime));
  Simulator::Run ();

  monitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowMonitor.GetClassifier ());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
      std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
      std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
      std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
      std::cout << "  Delay Sum: " << i->second.delaySum << "\n";
    }

  Simulator::Destroy ();
  return 0;
}