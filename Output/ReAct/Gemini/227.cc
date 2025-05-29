#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/aodv-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/spectrum-module.h"
#include "ns3/propagation-loss-model.h"
#include "ns3/propagation-delay-model.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("UAVNetwork");

int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponent::SetEnable ("UAVNetwork", LOG_LEVEL_INFO);

  int numUAVs = 5;
  double simDuration = 60.0;
  double dataRate = 1.0; // Mbps

  NodeContainer uavNodes;
  uavNodes.Create (numUAVs);

  NodeContainer gcsNode;
  gcsNode.Create (1);

  NodeContainer allNodes;
  allNodes.Add (uavNodes);
  allNodes.Add (gcsNode);

  WifiHelper wifiHelper;
  wifiHelper.SetStandard (WIFI_PHY_STANDARD_80211a);

  YansWifiChannelHelper channelHelper = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phyHelper = YansWifiPhyHelper::Default ();
  phyHelper.SetChannel (channelHelper.Create ());

  wifiHelper.SetRemoteStationManager ("ns3::AarfWifiManager");

  NetDeviceContainer uavDevices = wifiHelper.Install (phyHelper, uavNodes);
  NetDeviceContainer gcsDevice = wifiHelper.Install (phyHelper, gcsNode);

  AodvHelper aodv;
  InternetStackHelper stack;
  stack.SetRoutingProtocol ("ns3::AodvHelper", "EnableHello", BooleanValue(true));
  stack.Install (allNodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer uavInterfaces = address.Assign (uavDevices);
  Ipv4InterfaceContainer gcsInterface = address.Assign (gcsDevice);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::RandomBoxPositionAllocator",
                                 "X", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                 "Y", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                 "Z", StringValue ("ns3::UniformRandomVariable[Min=10.0|Max=20.0]"));

  mobility.SetMobilityModel ("ns3::RandomWalk3dMobilityModel",
                             "Mode", StringValue ("Time"),
                             "Time", StringValue ("1s"),
                             "Speed", StringValue ("ns3::ConstantRandomVariable[Constant=10.0]"));
  mobility.Install (uavNodes);

  MobilityHelper gcsMobility;
  gcsMobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  gcsMobility.Install (gcsNode);

  UdpEchoServerHelper echoServer (9);
  ApplicationContainer serverApps = echoServer.Install (gcsNode.Get (0));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (simDuration - 1.0));

  UdpEchoClientHelper echoClient (gcsInterface.GetAddress (0), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (1000));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (0.1)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps;
  for (int i = 0; i < numUAVs; ++i)
    {
      clientApps.Add (echoClient.Install (uavNodes.Get (i)));
    }

  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (simDuration - 2.0));

  FlowMonitorHelper flowMonitor;
  Ptr<FlowMonitor> monitor = flowMonitor.InstallAll ();

  Simulator::Stop (Seconds (simDuration));

  Simulator::Run ();

  monitor->CheckForLostPackets ();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowMonitor.GetClassifier ());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
      std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
      std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
      std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
      std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000000  << " Mbps\n";
    }

  monitor->SerializeToXmlFile("uav-network.flowmon", true, true);

  Simulator::Destroy ();
  return 0;
}