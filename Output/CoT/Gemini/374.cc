#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/lte-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/config-store.h"

using namespace ns3;

int main(int argc, char *argv[]) {

  CommandLine cmd;
  cmd.Parse(argc, argv);

  Config::SetDefault ("ns3::LteEnbComponentCarrier::DefaultDlEarfcn", UintegerValue (100));
  Config::SetDefault ("ns3::LteEnbComponentCarrier::DefaultUlEarfcn", UintegerValue (18100));

  NodeContainer enbNodes;
  enbNodes.Create(1);
  NodeContainer ueNodes;
  ueNodes.Create(1);

  LteHelper lteHelper;
  lteHelper.SetEnbDeviceAttribute ("DlEarfcn", UintegerValue (100));
  lteHelper.SetEnbDeviceAttribute ("UlEarfcn", UintegerValue (18100));

  PointToPointEpcHelper epcHelper;
  lteHelper.SetEpcHelper (&epcHelper);

  Ptr<Node> pgw = epcHelper.GetPgwNode ();

  // Create a single RemoteHost
  NodeContainer remoteHostContainer;
  remoteHostContainer.Create (1);
  Ptr<Node> remoteHost = remoteHostContainer.Get (0);
  InternetStackHelper internet;
  internet.Install (remoteHostContainer);

  // connect remote host to pgw
  PointToPointHelper p2ph;
  p2ph.SetDeviceAttribute ("DataRate", StringValue ("100Gb/s"));
  p2ph.SetDeviceAttribute ("Mtu", UintegerValue (1500));
  NetDeviceContainer dpd = p2ph.Install (pgw, remoteHost);

  Ipv4AddressHelper ipv4h;
  ipv4h.SetBase ("1.0.0.0", "255.255.255.0");
  Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign (dpd);
  Ipv4Address remoteHostAddr = internetIpIfaces.GetAddress (1);

  Ipv4StaticRoutingHelper ipv4RoutingHelper;
  Ptr<Ipv4StaticRouting> remoteHostStaticRouting = ipv4RoutingHelper.GetStaticRouting (remoteHost->GetObject<Ipv4> (0));
  remoteHostStaticRouting->AddNetworkRouteTo (Ipv4Address ("7.0.0.0"), Ipv4Mask ("255.0.0.0"), 1);

  NetDeviceContainer enbLteDevs = lteHelper.InstallEnbDevice (enbNodes);
  NetDeviceContainer ueLteDevs = lteHelper.InstallUeDevice (ueNodes);

  lteHelper.Attach (ueLteDevs, enbLteDevs.Get (0));

  // Install Mobility Model
  MobilityHelper enbMobility;
  enbMobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  enbMobility.Install (enbNodes);

  MobilityHelper ueMobility;
  ueMobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                                 "Mode", StringValue ("Time"),
                                 "Time", StringValue ("1s"),
                                 "Speed", StringValue ("1.0m/s"),
                                 "Bounds", RectangleValue (Rectangle (-100, 100, -100, 100)));
  ueMobility.Install (ueNodes);

  InternetStackHelper internetUe;
  internetUe.Install (ueNodes);
  Ipv4InterfaceContainer ueIpIface = epcHelper.AssignUeIpv4Address (NetDeviceContainer (ueLteDevs));
  ueNodes.Get (0)->GetObject<LteUeNetDevice> (0)->SetIpv4Address (ueIpIface.GetAddress (0));

  // Set the default gateway for the UE
  Ptr<Ipv4StaticRouting> ueStaticRouting = ipv4RoutingHelper.GetStaticRouting (ueNodes.Get (0)->GetObject<Ipv4> (0));
  ueStaticRouting->SetDefaultRoute (epcHelper.GetPgwNode ()->GetObject<Ipv4> (0)->GetAddress (1, 0), 1);

  // Install and start applications on UEs and remote host

  uint16_t dlPort = 1234;
  UdpClientHelper dlClient (ueIpIface.GetAddress (0), dlPort);
  dlClient.SetAttribute ("Interval", TimeValue (MilliSeconds (10)));
  dlClient.SetAttribute ("MaxPackets", UintegerValue (1000));

  ApplicationContainer dlClientApps = dlClient.Install (remoteHost);
  dlClientApps.Start (Seconds (1));
  dlClientApps.Stop (Seconds (10));

  UdpServerHelper dlPacketSink (dlPort);
  ApplicationContainer dlServerApps = dlPacketSink.Install (ueNodes.Get (0));
  dlServerApps.Start (Seconds (1));
  dlServerApps.Stop (Seconds (10));

  Simulator::Stop (Seconds (10));

  Simulator::Run ();

  Simulator::Destroy ();
  return 0;
}