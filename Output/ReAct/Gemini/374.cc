#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/lte-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/config-store-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  Config::SetDefault ("ns3::LteEnbRrc::DefaultTransmissionMode", UintegerValue (1));
  CommandLine cmd;
  cmd.Parse (argc, argv);

  NodeContainer enbNodes;
  enbNodes.Create (1);
  NodeContainer ueNodes;
  ueNodes.Create (1);

  LteHelper lteHelper;
  lteHelper.SetEnbDeviceAttribute ("DlEarfcn", UintegerValue (100));
  lteHelper.SetEnbDeviceAttribute ("UlEarfcn", UintegerValue (18100));

  PointToPointEpcHelper epcHelper;
  lteHelper.SetEpcHelper (&epcHelper);

  Ptr<Node> pgw = epcHelper.GetPgwNode ();

  // Create a remote host
  NodeContainer remoteHostContainer;
  remoteHostContainer.Create (1);
  Ptr<Node> remoteHost = remoteHostContainer.Get (0);
  InternetStackHelper internet;
  internet.Install (remoteHostContainer);

  // Create the Internet
  PointToPointHelper p2ph;
  p2ph.SetDeviceAttribute ("DataRate", DataRateValue (DataRate ("100Gb/s")));
  p2ph.SetDeviceAttribute ("Mtu", UintegerValue (1500));
  NetDeviceContainer internetDevices = p2ph.Install (pgw, remoteHost);
  Ipv4AddressHelper ipv4h;
  ipv4h.SetBase ("1.0.0.0", "255.0.0.0");
  Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign (internetDevices);
  Ipv4Address remoteHostAddr = internetIpIfaces.GetAddress (1);

  Ipv4StaticRoutingHelper ipv4RoutingHelper;
  Ptr<Ipv4StaticRouting> remoteHostStaticRouting = ipv4RoutingHelper.GetStaticRouting (remoteHost->GetObject<Ipv4> (0));
  remoteHostStaticRouting->AddNetworkRouteTo (Ipv4Address ("7.0.0.0"), Ipv4Mask ("255.0.0.0"), 1);

  NetDeviceContainer enbLteDevs = lteHelper.InstallEnbDevice (enbNodes);
  NetDeviceContainer ueLteDevs = lteHelper.InstallUeDevice (ueNodes);

  internet.Install (ueNodes);
  Ipv4InterfaceContainer ueIpIface = epcHelper.AssignUeIpv4Address (NetDeviceContainer (ueLteDevs));

  // set the default gateway for the UE
  Ptr<Node> ue = ueNodes.Get (0);
  Ptr<Ipv4StaticRouting> ueStaticRouting = ipv4RoutingHelper.GetStaticRouting (ue->GetObject<Ipv4> (0));
  ueStaticRouting->SetDefaultRoute (epcHelper.GetUeDefaultGatewayAddress (), 1);

  lteHelper.Attach (ueLteDevs.Get (0), enbLteDevs.Get (0));

  // set mobility
  MobilityHelper enbMobility;
  enbMobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  enbMobility.Install (enbNodes);

  MobilityHelper ueMobility;
  ueMobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                                "Mode", StringValue ("Time"),
                                "Time", StringValue ("2s"),
                                "Speed", StringValue ("1.0ms"),
                                "Bounds", RectangleValue (Rectangle (-100, 100, -100, 100)));
  ueMobility.Install (ueNodes);

  uint16_t dlPort = 1234;
  UdpClientHelper client (remoteHostAddr, dlPort);
  client.SetAttribute ("MaxPackets", UintegerValue (1000));
  client.SetAttribute ("Interval", TimeValue (MilliSeconds (1)));
  client.SetAttribute ("PacketSize", UintegerValue (1472));
  ApplicationContainer clientApps = client.Install (ueNodes.Get (0));
  clientApps.Start (Seconds (1));
  clientApps.Stop (Seconds (10));

  UdpServerHelper server (dlPort);
  ApplicationContainer serverApps = server.Install (remoteHost);
  serverApps.Start (Seconds (1));
  serverApps.Stop (Seconds (10));

  Simulator::Stop (Seconds (10));

  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}