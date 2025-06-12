#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main (int argc, char *argv[])
{
  Time::SetResolution (Time::NS);

  // Create nodes
  NodeContainer ueNodes;
  ueNodes.Create (3);
  NodeContainer enbNodes;
  enbNodes.Create (1);

  // Position allocation for eNodeB
  Ptr<ListPositionAllocator> enbPosAlloc = CreateObject<ListPositionAllocator> ();
  enbPosAlloc->Add (Vector (25.0, 25.0, 0.0));

  MobilityHelper enbMobility;
  enbMobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  enbMobility.SetPositionAllocator (enbPosAlloc);
  enbMobility.Install (enbNodes);

  // Setup UEs' random walk in 50x50 area
  MobilityHelper ueMobility;
  ueMobility.SetPositionAllocator ("ns3::RandomRectanglePositionAllocator",
                                   "X", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"),
                                   "Y", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"));
  ueMobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                               "Mode", StringValue ("Time"),
                               "Time", TimeValue (Seconds (1.0)),
                               "Speed", StringValue ("ns3::ConstantRandomVariable[Constant=1.0]"),
                               "Bounds", RectangleValue (Rectangle (0.0, 50.0, 0.0, 50.0)));
  ueMobility.Install (ueNodes);

  // Internet stack for UEs and remote host
  InternetStackHelper internet;
  internet.Install (ueNodes);

  // LTE set up
  Ptr<LteHelper> lteHelper = CreateObject<LteHelper> ();
  Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper> ();
  lteHelper->SetEpcHelper (epcHelper);

  // Create a remote host connected to EPC's PGW
  NodeContainer remoteHostContainer;
  remoteHostContainer.Create (1);
  Ptr<Node> remoteHost = remoteHostContainer.Get (0);
  InternetStackHelper internetHost;
  internetHost.Install (remoteHost);

  // Connect remoteHost to EPC
  Ptr<Node> pgw = epcHelper->GetPgwNode ();
  PointToPointHelper p2ph;
  p2ph.SetDeviceAttribute ("DataRate", DataRateValue (DataRate ("100Gbps")));
  p2ph.SetChannelAttribute ("Delay", TimeValue (Seconds (0.010)));
  NetDeviceContainer internetDevices = p2ph.Install (pgw, remoteHost);

  Ipv4AddressHelper ipv4h;
  ipv4h.SetBase ("1.0.0.0", "255.0.0.0");
  Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign (internetDevices);
  Ipv4Address remoteHostAddr = internetIpIfaces.GetAddress (1);

  // Enable routing on the remote host
  Ipv4StaticRoutingHelper ipv4RoutingHelper;
  Ptr<Ipv4StaticRouting> remoteHostStaticRouting = ipv4RoutingHelper.GetStaticRouting (remoteHost->GetObject<Ipv4> ());
  remoteHostStaticRouting->AddNetworkRouteTo (Ipv4Address ("7.0.0.0"), Ipv4Mask ("255.0.0.0"), 1);

  // Install LTE devices to the nodes
  NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice (enbNodes);
  NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice (ueNodes);

  // Assign IP addresses to UEs, set default gateway
  Ipv4InterfaceContainer ueIpIfaces;
  ueIpIfaces = epcHelper->AssignUeIpv4Address (NetDeviceContainer (ueLteDevs));

  for (uint32_t u = 0; u < ueNodes.GetN (); ++u)
    {
      Ptr<Node> ueNode = ueNodes.Get (u);
      Ptr<NetDevice> ueLteDev = ueLteDevs.Get (u);
      lteHelper->Attach (ueLteDev, enbLteDevs.Get (0));
      lteHelper->ActivateDedicatedEpsBearer (ueLteDev, EpsBearer (EpsBearer::GBR_CONV_VOICE), EpcTft::Default ());
      Ptr<Ipv4StaticRouting> ueStaticRouting = ipv4RoutingHelper.GetStaticRouting (ueNode->GetObject<Ipv4> ());
      ueStaticRouting->SetDefaultRoute (epcHelper->GetUeDefaultGatewayAddress (), 1);
    }

  // UDP server on eNodeB (via remote host)
  uint16_t port = 9;
  UdpServerHelper udpServer (port);
  ApplicationContainer serverApps = udpServer.Install (remoteHost);
  serverApps.Start (Seconds (0.0));
  serverApps.Stop (Seconds (10.0));

  // UDP clients on UEs
  for (uint32_t i = 0; i < ueNodes.GetN (); ++i)
    {
      UdpClientHelper udpClient (remoteHostAddr, port);
      udpClient.SetAttribute ("MaxPackets", UintegerValue (10));
      udpClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
      udpClient.SetAttribute ("PacketSize", UintegerValue (1024));
      ApplicationContainer clientApp = udpClient.Install (ueNodes.Get (i));
      clientApp.Start (Seconds (1.0));
      clientApp.Stop (Seconds (10.0));
    }

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}