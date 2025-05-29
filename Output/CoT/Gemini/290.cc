#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/lte-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/lte-phy.h"

using namespace ns3;

int main(int argc, char *argv[]) {

  Config::SetDefault ("ns3::LteUePhy::EnableUplinkPowerControl", BooleanValue (false));

  CommandLine cmd;
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponentEnable ("LteEnbRrc", LOG_LEVEL_ALL);
  LogComponentEnable ("LteUeRrc", LOG_LEVEL_ALL);
  LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);

  ConfigStore inputConfig;
  inputConfig.ConfigureDefaults ();

  NodeContainer enbNodes;
  enbNodes.Create (1);
  NodeContainer ueNodes;
  ueNodes.Create (1);

  LteHelper lteHelper;
  lteHelper.SetEnbAntennaModelType ("ns3::CosineAntennaModel");
  lteHelper.SetEnbDeviceAttribute ("DlEarfcn", UintegerValue (500));
  lteHelper.SetUeDeviceAttribute ("UlEarfcn", UintegerValue (23500));
  lteHelper.SetLteEnbDeviceAttribute ("TxPower", DoubleValue(30));

  PointToPointEpcHelper epcHelper;
  lteHelper.SetEpcHelper (&epcHelper);

  Ptr<Node> pgw = epcHelper.GetPgwNode ();

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
  NetDeviceContainer internetDevices = p2ph.Install (pgw, remoteHost);
  Ipv4AddressHelper ipv4h;
  ipv4h.SetBase ("1.0.0.0", "255.0.0.0");
  Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign (internetDevices);
  Ipv4Address remoteHostAddr = internetIpIfaces.GetAddress (1);

  Ipv4StaticRoutingHelper ipv4RoutingHelper;
  Ptr<Ipv4StaticRouting> remoteHostStaticRouting = ipv4RoutingHelper.GetStaticRouting (remoteHost->GetObject<Ipv4> (0));
  remoteHostStaticRouting->AddNetworkRouteTo (Ipv4Address ("7.0.0.0"), Ipv4Mask ("255.0.0.0"), 1);

  internet.Install (enbNodes);
  internet.Install (ueNodes);

  Ipv4InterfaceContainer ueIpIface;
  NetDeviceContainer enbLteDevs = lteHelper.InstallEnbDevice (enbNodes);
  NetDeviceContainer ueLteDevs = lteHelper.InstallUeDevice (ueNodes);
  ueIpIface = epcHelper.AssignUeIpv4Address (NetDeviceContainer (ueLteDevs));

  // set the default gateway for the UE
  Ptr<Node> ue = ueNodes.Get (0);
  Ptr<Ipv4StaticRouting> ueStaticRouting = ipv4RoutingHelper.GetStaticRouting (ue->GetObject<Ipv4> (0));
  ueStaticRouting->SetDefaultRoute (epcHelper.GetPgwNode ()->GetObject<Ipv4> (0)->GetAddress (1, 0).GetLocal (), 0, 0);

  Ipv4Address enbIpAddr = enbNodes.Get (0)->GetObject<Ipv4> (0)->GetAddress (1, 0).GetLocal ();

  // Attach one UE per eNodeB
  lteHelper.Attach (ueLteDevs.Get (0), enbLteDevs.Get (0));

  // Activate a data radio bearer between UE and eNodeB
  enum EpsBearer::Qci q = EpsBearer::GBR_QCI_BEARER;
  EpsBearer bearer (q);
  lteHelper.ActivateDataRadioBearer (ueNodes, bearer, enbLteDevs.Get (0));

  // Mobility model
  MobilityHelper mobility;
  mobility.SetMobilityModel ("ns3::RandomRectanglePositionAllocator",
                             "X", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                             "Y", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"));
  mobility.Install (ueNodes);

  Ptr<ConstantPositionMobilityModel> enbMobility = CreateObject<ConstantPositionMobilityModel> ();
  enbNodes.Get (0)->AggregateObject (enbMobility);
  Vector3D pos;
  pos.x = 50.0;
  pos.y = 50.0;
  pos.z = 0.0;
  enbMobility->SetPosition (pos);

  mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                             "Mode", StringValue ("Time"),
                             "Time", StringValue ("1s"),
                             "Speed", StringValue ("ns3::ConstantRandomVariable[Constant=1.0]"));
  mobility.Install (ueNodes);

  // UDP server on UE
  UdpServerHelper server (5000);
  ApplicationContainer serverApps = server.Install (ueNodes.Get (0));
  serverApps.Start (Seconds (0.0));
  serverApps.Stop (Seconds (10.0));

  // UDP client on eNodeB
  UdpClientHelper client (ueIpIface.GetAddress (0), 5000);
  client.SetAttribute ("MaxPackets", UintegerValue (4294967295));
  client.SetAttribute ("Interval", TimeValue (MilliSeconds (200)));
  client.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientApps = client.Install (enbNodes.Get (0));
  clientApps.Start (Seconds (1.0));
  clientApps.Stop (Seconds (10.0));

  // Tracing
  lteHelper.EnablePhyTraces ();
  lteHelper.EnableMacTraces ();
  lteHelper.EnableRlcTraces ();

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();

  Simulator::Destroy ();
  return 0;
}