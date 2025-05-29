#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/lte-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/config-store.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("LteUdpExample");

int main (int argc, char *argv[])
{
  // Set up command line arguments
  CommandLine cmd;
  cmd.Parse (argc, argv);

  // Enable logging
  LogComponentEnable ("LteUdpExample", LOG_LEVEL_INFO);

  // Create LTE helper
  Ptr<LteHelper> lteHelper = CreateObject<LteHelper> ();

  // Set eNodeB antenna model
  lteHelper->SetEnbAntennaModelType ("ns3::CosineAntennaModel");

  // Create EPC helper
  Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper> ();
  lteHelper->SetEpcHelper (epcHelper);

  // Create a remote host
  Ptr<Node> remoteHost = CreateObject<Node> ();
  InternetStackHelper internet;
  internet.Install (remoteHost);

  // Create the internet link between the remote host and the EPC
  PointToPointHelper p2ph;
  p2ph.SetDeviceAttribute ("DataRate", DataRateValue (DataRate ("1Gb/s")));
  p2ph.SetDeviceAttribute ("Mtu", UintegerValue (1500));
  NetDeviceContainer remoteHostNetDev = p2ph.Install (remoteHost, epcHelper->GetSpGwPgwNode ());
  Ipv4AddressHelper ipv4h;
  ipv4h.SetBase ("1.0.0.0", "255.0.0.0");
  Ipv4InterfaceContainer remoteHostIfaces = ipv4h.Assign (remoteHostNetDev);
  Ipv4Address remoteHostAddr = remoteHostIfaces.GetAddress (0);

  // Routing to make the remote host routable
  Ipv4StaticRoutingHelper ipv4RoutingHelper;
  Ptr<Ipv4StaticRouting> remoteHostStaticRouting = ipv4RoutingHelper.GetOrCreateRouting (remoteHost->GetObject<Ipv4> (0));
  remoteHostStaticRouting->AddNetworkRouteTo (epcHelper->GetPgwAddress (), Ipv4Mask ("255.255.0.0"), 1);

  // Create eNodeB and UE nodes
  NodeContainer enbNodes;
  enbNodes.Create (1);
  NodeContainer ueNodes;
  ueNodes.Create (3);

  // Install LTE devices to the nodes
  NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice (enbNodes);
  NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice (ueNodes);

  // Set mobility model
  MobilityHelper mobility;
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel"); // eNodeB is stationary

  // Install mobility model to eNodeB
  mobility.Install (enbNodes);
  Ptr<ConstantPositionMobilityModel> enbMobility = enbNodes.Get (0)->GetObject<ConstantPositionMobilityModel> ();
  enbMobility->SetPosition (Vector (50.0, 50.0, 0)); // Place eNodeB at the center

  // Set UE mobility
  mobility.SetMobilityModel ("ns3::RandomDirection2dMobilityModel",
                             "Bounds", RectangleValue (Rectangle (0, 100, 0, 100)),
                             "Speed", StringValue ("ns3::ConstantRandomVariable[Constant=1.0]")); // 1 m/s
  mobility.Install (ueNodes);

  // Attach UEs to the closest eNodeB
  lteHelper->Attach (ueLteDevs, enbLteDevs.Get (0));

  // Set active protocol stack for UEs
  internet.Install (ueNodes);
  Ipv4InterfaceContainer ueIpIface = epcHelper->AssignUeIpv4Address (NetDeviceContainer (ueLteDevs));

  // Assign IP address to UEs
  for (uint32_t u = 0; u < ueNodes.GetN (); ++u)
    {
      Ptr<Node> ueNode = ueNodes.Get (u);
      Ptr<Ipv4StaticRouting> ueStaticRouting = ipv4RoutingHelper.GetOrCreateRouting (ueNode->GetObject<Ipv4> (0));
      ueStaticRouting->SetDefaultRoute (epcHelper->GetUeDefaultGatewayAddress (), 1);
    }

  // Install applications: UDP server on UE 0
  uint16_t dlPort = 5000;
  UdpServerHelper dlserver (dlPort);
  ApplicationContainer dlserverApps = dlserver.Install (ueNodes.Get (0));
  dlserverApps.Start (Seconds (1.0));
  dlserverApps.Stop (Seconds (10.0));

  // Install applications: UDP client on UE 1
  uint32_t packetSize = 512;
  Time interPacketInterval = MilliSeconds (10);
  UdpClientHelper dlclient (ueIpIface.GetAddress (0), dlPort);
  dlclient.SetAttribute ("MaxPackets", UintegerValue (1000000));
  dlclient.SetAttribute ("Interval", TimeValue (interPacketInterval));
  dlclient.SetAttribute ("PacketSize", UintegerValue (packetSize));
  ApplicationContainer dlclientApps = dlclient.Install (ueNodes.Get (1));
  dlclientApps.Start (Seconds (2.0));
  dlclientApps.Stop (Seconds (10.0));

  // PCAP Tracing
  p2ph.EnablePcapAll ("lte-udp-example");
  lteHelper->EnableTraces ();

  // Flow monitor
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  // Run simulation
  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();

  // Print per flow statistics
  monitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();

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

  Simulator::Destroy ();

  return 0;
}