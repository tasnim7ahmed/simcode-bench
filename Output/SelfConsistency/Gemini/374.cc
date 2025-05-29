#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/lte-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/random-walk-2d-mobility-model.h"
#include "ns3/constant-position-mobility-model.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("LteUeUdp");

int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  // Enable logging
  LogComponentEnable ("LteUeUdp", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);

  // Create nodes: eNB and UE
  NodeContainer enbNodes;
  enbNodes.Create (1);
  NodeContainer ueNodes;
  ueNodes.Create (1);

  // Create LTE helper
  Ptr<LteHelper> lteHelper = CreateObject<LteHelper> ();

  // Create EPC helper
  Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper> ();
  lteHelper->SetEpcHelper (epcHelper);

  // Install LTE devices to the nodes
  NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice (enbNodes);
  NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice (ueNodes);

  // Mobility model for eNB (constant position)
  Ptr<ConstantPositionMobilityModel> enbMobility = CreateObject<ConstantPositionMobilityModel> ();
  enbNodes.Get (0)->AggregateObject (enbMobility);
  Vector enbPosition(0.0, 0.0, 0.0);
  enbMobility->SetPosition(enbPosition);

  // Mobility model for UE (random walk)
  Ptr<RandomWalk2dMobilityModel> ueMobility = CreateObject<RandomWalk2dMobilityModel> ();
  ueMobility->SetAttribute ("Bounds", RectangleValue (Rectangle (-100, 100, -100, 100)));
  ueNodes.Get (0)->AggregateObject (ueMobility);
  ueMobility->SetPosition(Vector (5.0, 5.0, 0.0)); //Starting point

  // Install the internet stack on the nodes
  InternetStackHelper internet;
  internet.Install (ueNodes);
  internet.Install (enbNodes);

  // Assign IP addresses
  Ipv4InterfaceContainer ueIpIface;
  Ipv4InterfaceContainer enbIpIface;
  Ipv4AddressHelper ipv4h;
  ipv4h.SetBase ("10.1.1.0", "255.255.255.0");
  ueIpIface = ipv4h.Assign (ueLteDevs);
  enbIpIface = ipv4h.Assign (enbLteDevs);

  // Set up the application (UDP client on UE, UDP server on eNB)
  uint16_t port = 9;  // Discard port (RFC 863)

  UdpClientHelper client (enbIpIface.GetAddress (0), port);
  client.SetAttribute ("MaxPackets", UintegerValue (1000));
  client.SetAttribute ("Interval", TimeValue (Seconds (0.01)));
  client.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientApps = client.Install (ueNodes.Get (0));
  clientApps.Start (Seconds (1.0));
  clientApps.Stop (Seconds (10.0));

  UdpServerHelper server (port);
  ApplicationContainer serverApps = server.Install (enbNodes.Get (0));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  // Attach UE to the eNB
  lteHelper->Attach (ueLteDevs.Get (0), enbLteDevs.Get (0));

  // Activate a data radio bearer
  enum EpsBearer::Qci q = EpsBearer::GBR_QCI_DEFAULT;
  EpsBearer bearer (q);
  lteHelper->ActivateDataRadioBearer (ueNodes, bearer, enbLteDevs.Get (0));

  // Flow monitor
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  // Run the simulation
  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();

  monitor->CheckForLostPackets ();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      NS_LOG_UNCOND("Flow ID: " << i->first << " Src Addr " << t.sourceAddress << " Dst Addr " << t.destinationAddress);
      NS_LOG_UNCOND("Tx Packets = " << i->second.txPackets);
      NS_LOG_UNCOND("Rx Packets = " << i->second.rxPackets);
      NS_LOG_UNCOND("Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024/1024  << " Mbps");
    }


  Simulator::Destroy ();

  return 0;
}