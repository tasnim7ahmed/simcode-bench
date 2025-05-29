/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/config-store-module.h"

using namespace ns3;

int
main(int argc, char *argv[])
{
  // Enable logging if needed:
  // LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
  // LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);

  // Create nodes: 1 eNodeB, 1 UE
  NodeContainer enbNodes;
  enbNodes.Create(1);

  NodeContainer ueNodes;
  ueNodes.Create(1);

  // Install Mobility
  Ptr<ListPositionAllocator> enbPosition = CreateObject<ListPositionAllocator> ();
  enbPosition->Add(Vector(0.0, 0.0, 0.0));
  MobilityHelper enbMobility;
  enbMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  enbMobility.SetPositionAllocator(enbPosition);
  enbMobility.Install(enbNodes);

  MobilityHelper ueMobility;
  ueMobility.SetPositionAllocator("ns3::RandomBoxPositionAllocator",
                                 "X", StringValue("ns3::UniformRandomVariable[Min=-50.0|Max=50.0]"),
                                 "Y", StringValue("ns3::UniformRandomVariable[Min=-50.0|Max=50.0]"),
                                 "Z", StringValue("ns3::ConstantRandomVariable[Constant=0.0]"));

  ueMobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(-50, 50, -50, 50)),
                              "Distance", DoubleValue(10.0),
                              "Speed", StringValue("ns3::ConstantRandomVariable[Constant=2.0]"));
  ueMobility.Install(ueNodes);

  // Install LTE Devices to Nodes
  Ptr<LteHelper> lteHelper = CreateObject<LteHelper> ();
  Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper> ();
  lteHelper->SetEpcHelper(epcHelper);

  NetDeviceContainer enbDevs = lteHelper->InstallEnbDevice(enbNodes);
  NetDeviceContainer ueDevs = lteHelper->InstallUeDevice(ueNodes);

  // Install the IP stack on the UEs
  InternetStackHelper internet;
  internet.Install(ueNodes);

  // Assign IP address to UEs
  Ipv4InterfaceContainer ueIpIface;
  ueIpIface = epcHelper->AssignUeIpv4Address (NetDeviceContainer (ueDevs));

  // Attach UE to eNodeB
  lteHelper->Attach(ueDevs.Get(0), enbDevs.Get(0));

  // Set up routing for UE
  // (not strictly needed for echo application, but for completeness)
  Ptr<Node> ueNode = ueNodes.Get(0);
  Ptr<Ipv4StaticRouting> ueStaticRouting = Ipv4RoutingHelper::GetRouting <Ipv4StaticRouting> (ueNode->GetObject<Ipv4> ()->GetRoutingProtocol ());
  ueStaticRouting->SetDefaultRoute (epcHelper->GetUeDefaultGatewayAddress(), 1);

  // Add Internet to the remote host
  Ptr<Node> remoteHost;
  NodeContainer remoteHostContainer;
  remoteHostContainer.Create(1);
  remoteHost = remoteHostContainer.Get(0);
  InternetStackHelper internetHost;
  internetHost.Install(remoteHostContainer);

  // Connect remote host to PGW
  Ptr<Node> pgw = epcHelper->GetPgwNode ();
  PointToPointHelper p2ph;
  p2ph.SetDeviceAttribute ("DataRate", DataRateValue (DataRate ("100Gb/s")));
  p2ph.SetChannelAttribute ("Delay", TimeValue (Seconds (0.010)));

  NetDeviceContainer internetDevices = p2ph.Install (pgw, remoteHost);
  Ipv4AddressHelper ipv4h;
  ipv4h.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer internetIfaces = ipv4h.Assign (internetDevices);

  // Set up routing at remote host
  Ipv4StaticRoutingHelper ipv4RoutingHelper;
  Ptr<Ipv4StaticRouting> remoteHostStaticRouting = ipv4RoutingHelper.GetStaticRouting (remoteHost->GetObject<Ipv4> ());
  remoteHostStaticRouting->AddNetworkRouteTo (Ipv4Address ("7.0.0.0"), Ipv4Mask ("255.0.0.0"), 1);

  // UDP Echo server on UE node, port 9
  uint16_t echoPort = 9;
  UdpEchoServerHelper echoServer (echoPort);
  ApplicationContainer serverApps = echoServer.Install(ueNodes.Get(0));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  // UDP Echo client on remote host (to UE's assigned IP)
  UdpEchoClientHelper echoClient (ueIpIface.GetAddress(0), echoPort);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (10));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (512));

  ApplicationContainer clientApps = echoClient.Install(remoteHost);
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  // Enable Packet Capture
  p2ph.EnablePcapAll("lte-single-cell");

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}