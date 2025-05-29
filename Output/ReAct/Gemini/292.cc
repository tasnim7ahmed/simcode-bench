#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"
#include "ns3/lte-module.h"
#include "ns3/epc-helper.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {

  Config::SetDefault ("ns3::LteUePhy::EnableUplinkPowerControl", BooleanValue (false));

  CommandLine cmd;
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);
  LogComponentEnable ("EpcSgwPgwApplication", LOG_LEVEL_INFO);
  LogComponentEnable ("LteUeRrc", LOG_LEVEL_ALL);
  LogComponentEnable ("LteEnbRrc", LOG_LEVEL_ALL);

  NodeContainer enbNodes;
  enbNodes.Create (1);
  NodeContainer ueNodes;
  ueNodes.Create (1);

  // Create EPC Helper
  Ptr<LteHelper> lteHelper = CreateObject<LteHelper> ();
  Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper> ();
  lteHelper->SetEpcHelper (epcHelper);

  // Create a single RemoteHost
  NodeContainer remoteHostContainer;
  remoteHostContainer.Create (1);
  Ptr<Node> remoteHost = remoteHostContainer.Get (0);
  InternetStackHelper internet;
  internet.Install (remoteHostContainer);

  // Create the internet
  PointToPointHelper p2ph;
  p2ph.SetDeviceAttribute ("DataRate", StringValue ("100Gb/s"));
  p2ph.SetDeviceAttribute ("Mtu", UintegerValue (1500));
  p2ph.SetChannelAttribute ("Delay", StringValue ("1ms"));
  NetDeviceContainer internetDevices = p2ph.Install (remoteHost, epcHelper->GetPgwNode ());
  Ipv4AddressHelper ipv4h;
  ipv4h.SetBase ("1.0.0.0", "255.255.255.0");
  Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign (internetDevices);
  Ipv4Address remoteHostAddr = internetIpIfaces.GetAddress (0);

  // Routing to internet
  Ipv4StaticRoutingHelper ipv4RoutingHelper;
  Ptr<Ipv4StaticRouting> remoteHostStaticRouting = ipv4RoutingHelper.GetStaticRouting (remoteHost->GetObject<Ipv4> (0));
  remoteHostStaticRouting->AddNetworkRouteTo (epcHelper->GetPgwAddr (), Ipv4Mask ("255.255.0.0"), 1);

  // Create eNB and UE
  MobilityHelper enbMobility;
  enbMobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  enbMobility.Install (enbNodes);

  MobilityHelper ueMobility;
  ueMobility.SetPositionAllocator ("ns3::RandomDiscPositionAllocator",
                                   "X", DoubleValue (0.0),
                                   "Y", DoubleValue (0.0),
                                   "Rho", StringValue ("ns3::UniformRandomVariable[Min=20.0|Max=30.0]"));
  ueMobility.SetMobilityModel ("ns3::ConstantVelocityMobilityModel");
  ueMobility.Install (ueNodes);
  ueNodes.Get(0)->GetObject<MobilityModel>()->SetVelocity(Vector(10, 0, 0)); // 10 m/s in x direction

  // Install LTE Devices to the nodes
  NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice (enbNodes);
  NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice (ueNodes);

  // Attach the UE to the eNB
  lteHelper->Attach (ueLteDevs.Get (0), enbLteDevs.Get (0));

  // Set Active UEs
  lteHelper->ActivateUeContext (ueLteDevs, 1);

  // Install internet stack on the UE
  internet.Install (ueNodes);
  Ipv4InterfaceContainer ueIpIface = epcHelper->AssignUeIpv4Address (NetDeviceContainer (ueLteDevs));

  // Set the default gateway for the UE
  Ptr<Node> ue = ueNodes.Get (0);
  Ptr<Ipv4StaticRouting> ueStaticRouting = ipv4RoutingHelper.GetStaticRouting (ue->GetObject<Ipv4> (0));
  ueStaticRouting->SetDefaultRoute (epcHelper->GetUeDefaultGatewayAddress (), 1);

  // Create Applications
  uint16_t dlPort = 10000;
  UdpServerHelper dlPacketSink (dlPort);
  ApplicationContainer dlSinkApps = dlPacketSink.Install (enbNodes.Get (0));
  dlSinkApps.Start (Seconds (1.0));
  dlSinkApps.Stop (Seconds (10.0));

  uint16_t ulPort = 20000;
  UdpClientHelper ulPacketSource (ueIpIface.GetAddress (0), ulPort);
  ulPacketSource.SetAttribute ("PacketSize", UintegerValue (512));
  ulPacketSource.SetAttribute ("Interval", TimeValue (MilliSeconds (500)));
  ApplicationContainer ulClientApps = ulPacketSource.Install (ueNodes.Get (0));
  ulClientApps.Start (Seconds (2.0));
  ulClientApps.Stop (Seconds (10.0));

  Simulator::Stop (Seconds (10.0));

  Simulator::Run ();

  Simulator::Destroy ();
  return 0;
}