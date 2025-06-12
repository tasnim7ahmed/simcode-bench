/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Simulation: Two UEs communicate via a single eNodeB and EPC, using TCP.
 * NetAnim visualization enabled.
 */
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/lte-module.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("LteTcpEpcNetAnimExample");

int main (int argc, char *argv[])
{
  // Enable logging if needed
  // LogComponentEnable ("LteTcpEpcNetAnimExample", LOG_LEVEL_INFO);

  // Simulation parameters
  double simTime = 5.0; // seconds

  // Create nodes: 1 eNodeB, 2 UEs
  NodeContainer ueNodes;
  ueNodes.Create (2);
  NodeContainer enbNodes;
  enbNodes.Create (1);

  // Install mobility for visualization
  MobilityHelper mobility;
  // eNodeB at (50,50)
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (enbNodes);
  Ptr<MobilityModel> enbMobility = enbNodes.Get (0)->GetObject<MobilityModel> ();
  enbMobility->SetPosition (Vector (50.0, 50.0, 0.0));

  // UEs at (30,50) and (70,50)
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (ueNodes);
  ueNodes.Get (0)->GetObject<MobilityModel> ()->SetPosition (Vector (30.0, 50.0, 0.0));
  ueNodes.Get (1)->GetObject<MobilityModel> ()->SetPosition (Vector (70.0, 50.0, 0.0));

  // LTE Helper and EPC
  Ptr<LteHelper> lteHelper = CreateObject<LteHelper> ();
  Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper> ();
  lteHelper->SetEpcHelper (epcHelper);

  // Install LTE Devices to eNodeB and UEs
  NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice (enbNodes);
  NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice (ueNodes);

  // Install the IP stack on the UEs
  InternetStackHelper internet;
  internet.Install (ueNodes);

  // Assign IP address to UEs, retrieved from EPC's PGW
  Ipv4InterfaceContainer ueIpIfaces;
  ueIpIfaces = epcHelper->AssignUeIpv4Address (NetDeviceContainer (ueLteDevs));

  // Attach UEs to the eNodeB
  for (uint32_t i = 0; i < ueNodes.GetN (); ++i)
    {
      lteHelper->Attach (ueLteDevs.Get (i), enbLteDevs.Get (0));
    }

  // Set up default gateway for UEs
  for (uint32_t i = 0; i < ueNodes.GetN (); ++i)
    {
      Ptr<Node> ueNode = ueNodes.Get (i);
      Ptr<Ipv4StaticRouting> ueStaticRouting = Ipv4RoutingHelper::GetRouting <Ipv4StaticRouting> (
            ueNode->GetObject<Ipv4> ()->GetRoutingProtocol ());
      ueStaticRouting->SetDefaultRoute (epcHelper->GetUeDefaultGatewayAddress (), 1);
    }

  // Setup TCP applications between UEs
  uint16_t port = 5000;
  // Install a PacketSink (TCP server) on UE1 (received by UE1)
  Address sinkAddress (InetSocketAddress (ueIpIfaces.GetAddress (1), port));
  PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", sinkAddress);
  ApplicationContainer sinkApp = packetSinkHelper.Install (ueNodes.Get (1));
  sinkApp.Start (Seconds (0.1));
  sinkApp.Stop (Seconds (simTime));

  // Install a TCP OnOffApplication (client) on UE0
  OnOffHelper clientHelper ("ns3::TcpSocketFactory", sinkAddress);
  clientHelper.SetAttribute ("DataRate", DataRateValue (DataRate ("1Mbps")));
  clientHelper.SetAttribute ("PacketSize", UintegerValue (1000));
  clientHelper.SetAttribute ("StartTime", TimeValue (Seconds (1.0)));
  clientHelper.SetAttribute ("StopTime", TimeValue (Seconds (simTime)));

  ApplicationContainer clientApp = clientHelper.Install (ueNodes.Get (0));

  // NetAnim Setup
  AnimationInterface anim ("lte-tcp-epc-netanim.xml");
  // Assign node descriptions and colors for clarity
  anim.UpdateNodeDescription (enbNodes.Get (0), "eNodeB");
  anim.UpdateNodeColor (enbNodes.Get (0), 255, 0, 0);
  anim.UpdateNodeDescription (ueNodes.Get (0), "UE0");
  anim.UpdateNodeColor (ueNodes.Get (0), 0, 255, 0);
  anim.UpdateNodeDescription (ueNodes.Get (1), "UE1");
  anim.UpdateNodeColor (ueNodes.Get (1), 0, 0, 255);

  // Set random seed for reproducibility if desired
  // RngSeedManager::SetSeed (12345);

  Simulator::Stop (Seconds (simTime));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}