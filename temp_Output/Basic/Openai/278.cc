#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("SimpleLteHttpSimulation");

int
main (int argc, char *argv[])
{
  Time::SetResolution (Time::NS);

  // Create one eNodeB and one UE
  NodeContainer enbNodes, ueNodes;
  enbNodes.Create (1);
  ueNodes.Create (1);

  // Mobility for eNodeB (static)
  MobilityHelper mobilityEnb;
  mobilityEnb.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobilityEnb.SetPositionAllocator ("ns3::ListPositionAllocator",
    "PositionList", VectorValue (std::vector<Vector> {Vector (25.0, 25.0, 0.0)}));
  mobilityEnb.Install (enbNodes);

  // Mobility for UE (RandomWalk2d)
  MobilityHelper mobilityUe;
  mobilityUe.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
      "Bounds", RectangleValue (Rectangle (0.0, 50.0, 0.0, 50.0)),
      "Distance", DoubleValue (10.0),
      "Speed", StringValue ("ns3::ConstantRandomVariable[Constant=5.0]"));
  mobilityUe.SetPositionAllocator ("ns3::RandomRectanglePositionAllocator",
                                   "X", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"),
                                   "Y", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"));
  mobilityUe.Install (ueNodes);

  // LTE Devices
  Ptr<LteHelper> lteHelper = CreateObject<LteHelper> ();
  NetDeviceContainer enbDevs = lteHelper->InstallEnbDevice (enbNodes);
  NetDeviceContainer ueDevs = lteHelper->InstallUeDevice (ueNodes);

  // EPC, PGW, Internet
  Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper> ();
  lteHelper->SetEpcHelper (epcHelper);
  Ptr<Node> pgw = epcHelper->GetPgwNode ();

  // Create a remote host and connect to PGW
  NodeContainer remoteHostContainer;
  remoteHostContainer.Create (1);
  Ptr<Node> remoteHost = remoteHostContainer.Get (0);
  InternetStackHelper internet;
  internet.Install (remoteHostContainer);

  // Create P2P link PGW <-> remoteHost
  PointToPointHelper p2ph;
  p2ph.SetDeviceAttribute ("DataRate", DataRateValue (DataRate ("100Gbps")));
  p2ph.SetChannelAttribute ("Delay", TimeValue (Seconds (0.010)));
  NetDeviceContainer internetDevices = p2ph.Install (pgw, remoteHost);

  Ipv4AddressHelper ipv4h;
  ipv4h.SetBase ("1.0.0.0", "255.0.0.0");
  Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign (internetDevices);

  // Install IP stack on UEs
  internet.Install (ueNodes);

  Ipv4InterfaceContainer ueIpIfaces;
  ueIpIfaces = epcHelper->AssignUeIpv4Address (NetDeviceContainer (ueDevs));
  lteHelper->Attach (ueDevs.Get (0), enbDevs.Get (0));

  // Set default route for UE
  Ptr<Ipv4StaticRouting> ueStaticRouting = Ipv4RoutingHelper::GetRouting <Ipv4StaticRouting> (
      ueNodes.Get (0)->GetObject<Ipv4> ()->GetRoutingProtocol ());
  ueStaticRouting->SetDefaultRoute (epcHelper->GetUeDefaultGatewayAddress (), 1);

  // Install UDP server at eNodeB
  uint16_t httpPort = 80;
  Ipv4Address enbAddr = epcHelper->GetUeDefaultGatewayAddress ();
  UdpServerHelper udpServer (httpPort);
  ApplicationContainer serverApps = udpServer.Install (enbNodes.Get (0));
  serverApps.Start (Seconds (0.0));
  serverApps.Stop (Seconds (10.0));

  // Install UDP client ("HTTP client" sending 5 packets, interval 1s) at UE
  UdpClientHelper udpClient (enbAddr, httpPort);
  udpClient.SetAttribute ("MaxPackets", UintegerValue (5));
  udpClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  udpClient.SetAttribute ("PacketSize", UintegerValue (64));
  ApplicationContainer clientApps = udpClient.Install (ueNodes.Get (0));
  clientApps.Start (Seconds (1.0));
  clientApps.Stop (Seconds (10.0));

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}