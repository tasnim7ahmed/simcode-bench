/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/nr-module.h"
#include "ns3/antenna-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("NrOneGnbOneUeExample");

int
main (int argc, char *argv[])
{
  // Set simulation parameters
  double simTime = 10.0;
  uint32_t packetSize = 512;
  uint32_t numPackets = 5;
  double interPacketInterval = 1.0; // seconds

  // Enable logging (uncomment for more details)
  //LogComponentEnable ("NrOneGnbOneUeExample", LOG_LEVEL_INFO);
  //LogComponentEnable ("NrHelper", LOG_LEVEL_INFO);

  // Create nodes: Node 0 = gNB, Node 1 = UE
  NodeContainer enbNodes;
  enbNodes.Create (1);
  NodeContainer ueNodes;
  ueNodes.Create (1);

  // Install Mobility for gNB: Fixed position
  Ptr<ListPositionAllocator> enbPositionAlloc = CreateObject<ListPositionAllocator> ();
  enbPositionAlloc->Add (Vector (50.0, 50.0, 1.5)); // Center of the area
  MobilityHelper enbMobility;
  enbMobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  enbMobility.SetPositionAllocator (enbPositionAlloc);
  enbMobility.Install (enbNodes);

  // Install Mobility for UE: Start at a random position, move with random walk
  MobilityHelper ueMobility;
  ueMobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                               "Bounds", RectangleValue (Rectangle (0.0, 100.0, 0.0, 100.0)),
                               "Distance", DoubleValue (20.0),
                               "Speed", StringValue ("ns3::ConstantRandomVariable[Constant=2.0]")
                              );
  ueMobility.SetPositionAllocator ("ns3::UniformDiscPositionAllocator",
                                   "X", DoubleValue (50.0),
                                   "Y", DoubleValue (50.0),
                                   "Rho", DoubleValue (50.0)); // Place randomly in 100x100
  ueMobility.Install (ueNodes);

  // Install NR devices
  Ptr<NrHelper> nrHelper = CreateObject<NrHelper> ();
  nrHelper->SetAttribute ("BeamformingMethod", StringValue ("cellScan"));
  // Use default EPC configuration (Core Network)
  Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper> ();
  nrHelper->SetEpcHelper (epcHelper);

  // Spectrum configuration (use ITU B2100 MHz band as an example)
  nrHelper->SetNrSpectrumChannelType ("ns3::MultiModelSpectrumChannel");
  double centralFrequency = 2e9; // 2 GHz
  double bandwidth = 20e6; // 20 MHz

  BandwidthPartInfoPtrVector allBwps;
  allBwps = CcBwpCreator::CreateOneCcOneBwp (centralFrequency, bandwidth, BandwidthPartInfo::UMi_StreetCanyon);
  nrHelper->InitializeOperationBand (&allBwps);

  // Install NR devices to gNB and UEs
  NetDeviceContainer enbNetDev = nrHelper->InstallGnbDevice (enbNodes, allBwps);
  NetDeviceContainer ueNetDev = nrHelper->InstallUeDevice (ueNodes, allBwps);

  // Interface S1-U between gNB and EPC
  Ptr<Node> pgw = epcHelper->GetPgwNode ();

  // Install the Internet stack on the UE
  InternetStackHelper internet;
  internet.Install (ueNodes);

  // Assign IP addresses to UE devices
  Ipv4InterfaceContainer ueIpIface;
  ueIpIface = epcHelper->AssignUeIpv4Address (NetDeviceContainer (ueNetDev));

  // Attach UE to the gNB
  nrHelper->AttachToClosestEnb (ueNetDev, enbNetDev);

  // Add default route for UE
  for (uint32_t i = 0; i < ueNodes.GetN (); ++i)
    {
      Ptr<Node> ueNode = ueNodes.Get (i);
      Ptr<Ipv4StaticRouting> ueStaticRouting = Ipv4RoutingHelper::GetRouting <Ipv4StaticRouting> (ueNode->GetObject<Ipv4> ()->GetRoutingProtocol ());
      ueStaticRouting->SetDefaultRoute (epcHelper->GetUeDefaultGatewayAddress (), 1);
    }

  // Install Internet stack on gNB's side (remote host)
  NodeContainer remoteHostContainer;
  remoteHostContainer.Create (1);
  Ptr<Node> remoteHost = remoteHostContainer.Get (0);

  InternetStackHelper internetHost;
  internetHost.Install (remoteHost);

  // Create a point-to-point link between PGW and remote host
  PointToPointHelper p2ph;
  p2ph.SetDeviceAttribute ("DataRate", DataRateValue (DataRate ("10Gbps")));
  p2ph.SetChannelAttribute ("Delay", TimeValue (Seconds (0.001)));

  NetDeviceContainer internetDevices = p2ph.Install (pgw, remoteHost);

  Ipv4AddressHelper ipv4h;
  ipv4h.SetBase ("1.0.0.0", "255.0.0.0");
  Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign (internetDevices);

  // Set remote host as default gateway for traffic from PGW
  Ptr<Ipv4StaticRouting> hostStaticRouting = Ipv4RoutingHelper::GetRouting<Ipv4StaticRouting>
    (remoteHost->GetObject<Ipv4> ()->GetRoutingProtocol ());
  hostStaticRouting->AddNetworkRouteTo (Ipv4Address ("7.0.0.0"), Ipv4Mask ("255.0.0.0"), 1);

  // TCP server: On remote host, will receive traffic to TCP port (50000)
  uint16_t serverPort = 50000;
  Address serverAddress (InetSocketAddress (internetIpIfaces.GetAddress (1), serverPort));
  PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", serverAddress);
  ApplicationContainer serverApps = packetSinkHelper.Install (remoteHost);
  serverApps.Start (Seconds (0.0));
  serverApps.Stop (Seconds (simTime));

  // TCP client: On UE, connects to remote host server, 5 packets, 512 bytes, 1s intervals
  OnOffHelper clientHelper ("ns3::TcpSocketFactory", serverAddress);
  clientHelper.SetAttribute ("PacketSize", UintegerValue (packetSize));
  clientHelper.SetAttribute ("MaxBytes", UintegerValue (packetSize * numPackets));
  clientHelper.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1.0]"));
  clientHelper.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0.0]"));
  clientHelper.SetAttribute ("DataRate", DataRateValue (DataRate ((packetSize * 8) / interPacketInterval + 1000))); // Add small offset to allow sending

  ApplicationContainer clientApps = clientHelper.Install (ueNodes.Get (0));
  clientApps.Start (Seconds (1.0));
  clientApps.Stop (Seconds (simTime));

  // Enable packet capture on interfaces
  p2ph.EnablePcapAll ("nr-onoff");

  Simulator::Stop (Seconds (simTime));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}