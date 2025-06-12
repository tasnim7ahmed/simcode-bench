#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/lte-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/config-store.h"

using namespace ns3;

int main(int argc, char *argv[]) {

  CommandLine cmd;
  cmd.Parse(argc, argv);

  ConfigStore inputConfig;
  inputConfig.ConfigureDefaults();

  NodeContainer enbNodes;
  enbNodes.Create(1);
  NodeContainer ueNodes;
  ueNodes.Create(1);

  LteHelper lteHelper;
  lteHelper.SetEnbDeviceAttribute("DlEarfcn", UintegerValue(500));
  lteHelper.SetEnbDeviceAttribute("UlEarfcn", UintegerValue(18500));

  NetDeviceContainer enbDevs;
  enbDevs = lteHelper.InstallEnbDevice(enbNodes);
  NetDeviceContainer ueDevs;
  ueDevs = lteHelper.InstallUeDevice(ueNodes);

  Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
  lteHelper.SetEpcHelper(epcHelper);

  Ptr<Node> pgw = epcHelper->GetPgwNode();
  InternetStackHelper internet;
  internet.Install(pgw);

  // Create a single RemoteHost
  NodeContainer remoteHostContainer;
  remoteHostContainer.Create (1);
  Ptr<Node> remoteHost = remoteHostContainer.Get (0);
  internet.Install (remoteHost);

  // Connect the PGW to the remote host with a 100Mbps link
  PointToPointHelper p2ph;
  p2ph.SetDeviceAttribute ("DataRate", DataRateValue (DataRate ("100Gb/s")));
  p2ph.SetDeviceAttribute ("Delay", TimeValue (MilliSeconds (1)));
  NetDeviceContainer pgwP2pDevs;
  pgwP2pDevs = p2ph.Install (pgw, remoteHost);

  Ipv4AddressHelper ipv4h;
  ipv4h.SetBase ("1.0.0.0", "255.0.0.0");
  Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign (pgwP2pDevs);
  // interface 0 is localhost, 1 is the p2p device
  Ipv4Address remoteHostAddr = internetIpIfaces.GetAddress (1);

  Ipv4StaticRoutingHelper ipv4RoutingHelper;
  Ptr<Ipv4StaticRouting> remoteHostStaticRouting = ipv4RoutingHelper.GetStaticRouting (remoteHost->GetObject<Ipv4> ());
  remoteHostStaticRouting->AddNetworkRouteTo (Ipv4Address ("7.0.0.0"), Ipv4Mask ("255.0.0.0"), 1);

  internet.Install(ueNodes);
  internet.Install(enbNodes);

  Ipv4InterfaceContainer ueIpIface;
  ueIpIface = epcHelper->AssignUeIpv4Address (NetDeviceContainer (ueDevs));

  Ipv4Address ueAddr = ueIpIface.GetAddress (0);

  // Set the default gateway for the UE
  Ptr<Node> ueNode = ueNodes.Get (0);
  Ptr<Ipv4StaticRouting> ueStaticRouting = ipv4RoutingHelper.GetStaticRouting (ueNode->GetObject<Ipv4> ());
  ueStaticRouting->SetDefaultRoute (epcHelper->GetPgwAddress (), 1);

  Ipv4Address enbAddr;
  for (uint32_t i = 0; i < enbNodes.GetN (); ++i)
    {
      Ptr<Node> enbNode = enbNodes.Get (i);
      Ptr<Ipv4> ipv4 = enbNode->GetObject<Ipv4> ();
      enbAddr = ipv4->GetAddress (1, 0).GetLocal ();
      Ptr<Ipv4StaticRouting> enbStaticRouting = ipv4RoutingHelper.GetStaticRouting (enbNode->GetObject<Ipv4> ());
      enbStaticRouting->AddNetworkRouteTo (Ipv4Address ("1.0.0.0"), Ipv4Mask ("255.0.0.0"), 1);
    }

  MobilityHelper mobility;
  mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                            "Bounds", RectangleValue(Rectangle(-50, 50, -50, 50)));
  mobility.Install(ueNodes);
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(enbNodes);

  lteHelper.Attach (ueDevs, enbDevs.Get (0));

  UdpEchoServerHelper echoServer (9);
  ApplicationContainer serverApps = echoServer.Install (ueNodes.Get (0));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  UdpEchoClientHelper echoClient (ueAddr, 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (10));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (512));
  ApplicationContainer clientApps = echoClient.Install (enbNodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}