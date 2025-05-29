#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/nr-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("NrSimpleTcp");

int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  LogComponent::SetLogLevel (LogComponent::GetComponent ("NrUe"), LOG_LEVEL_ALL);
  LogComponent::SetLogLevel (LogComponent::GetComponent ("NrGnb"), LOG_LEVEL_ALL);

  // Create Nodes: gNB and UEs
  NodeContainer gnbNodes;
  gnbNodes.Create (1);
  NodeContainer ueNodes;
  ueNodes.Create (2);

  // Mobility Model: Random Walk for UEs
  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::RandomBoxPositionAllocator",
                                 "X", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"),
                                 "Y", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"),
                                 "Z", StringValue ("ns3::ConstantRandomVariable[Constant=0.0]"));
  mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                             "Bounds", RectangleValue (Rectangle (0, 50, 0, 50)));
  mobility.Install (ueNodes);

  // Static Position for gNB
  Ptr<ListPositionAllocator> gnbPositionAlloc = CreateObject<ListPositionAllocator> ();
  gnbPositionAlloc->Add (Vector (25.0, 25.0, 0.0));
  mobility.SetPositionAllocator (gnbPositionAlloc);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (gnbNodes);

  // Install NR devices
  NrHelper nrHelper;
  NetDeviceContainer gnbDevs = nrHelper.InstallGnbDevice (gnbNodes);
  NetDeviceContainer ueDevs = nrHelper.InstallUeDevice (ueNodes);

  // Attach UEs to the closest gNB
  nrHelper.AttachToClosestEnb (ueDevs, gnbDevs);

  // Install Internet Stack
  InternetStackHelper internet;
  internet.Install (gnbNodes);
  internet.Install (ueNodes);

  // Assign IP Addresses
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer gnbIpIface = ipv4.Assign (gnbDevs);
  Ipv4InterfaceContainer ueIpIface = ipv4.Assign (ueDevs);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // Create TCP application: Client on UE0, Server on UE1
  uint16_t port = 8080;
  Address serverAddress (InetSocketAddress (ueIpIface.GetAddress (1), port));

  // Configure TCP client (UE0)
  TcpClientHelper clientHelper (serverAddress);
  clientHelper.SetAttribute ("PacketSize", UintegerValue (1024));
  clientHelper.SetAttribute ("MaxPackets", UintegerValue (1000)); // Limit the number of packets
  clientHelper.SetAttribute ("RemotePort", UintegerValue (port));

  ApplicationContainer clientApps = clientHelper.Install (ueNodes.Get (0));
  clientApps.Start (Seconds (1.0));
  clientApps.Stop (Seconds (10.0));

  // Configure TCP server (UE1)
  TcpServerHelper serverHelper (port);
  ApplicationContainer serverApps = serverHelper.Install (ueNodes.Get (1));
  serverApps.Start (Seconds (0.0));
  serverApps.Stop (Seconds (10.0));

  // Flow Monitor
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  // Simulation time
  Simulator::Stop (Seconds (10.0));

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
      std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
      std::cout << "  Delay Sum:    " << i->second.delaySum << "\n";
      std::cout << "  Jitter Sum:   " << i->second.jitterSum << "\n";
    }

  monitor->SerializeToXmlFile("nr-tcp-flowmon.xml", true, true);

  Simulator::Destroy ();

  return 0;
}