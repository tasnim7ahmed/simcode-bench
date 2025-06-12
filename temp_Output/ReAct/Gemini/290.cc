#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/lte-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/config-store-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  Config::SetDefault ("ns3::LteUePhy::TxPower", DoubleValue (30));

  CommandLine cmd;
  cmd.Parse (argc, argv);

  NodeContainer enbNodes;
  enbNodes.Create (1);
  NodeContainer ueNodes;
  ueNodes.Create (1);

  LteHelper lteHelper;
  lteHelper.SetEnbAntennaModelType ("ns3::CosineAntennaModel");
  lteHelper.SetEnbDeviceAttribute ("DlEarfcn", UintegerValue (50));

  Point2D enbPosition(50, 50);

  MobilityHelper enbMobility;
  enbMobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  enbMobility.Install (enbNodes);
  Ptr<ConstantPositionMobilityModel> enbMob = enbNodes.Get(0)->GetObject<ConstantPositionMobilityModel>();
  enbMob->SetPosition(Vector(enbPosition.x, enbPosition.y, 0));

  MobilityHelper ueMobility;
  ueMobility.SetPositionAllocator ("ns3::RandomBoxPositionAllocator",
                                   "X", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                   "Y", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                   "Z", StringValue ("ns3::ConstantRandomVariable[Constant=0.0]"));
  ueMobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                               "Mode", StringValue ("Time"),
                               "Time", StringValue ("1s"),
                               "Speed", StringValue ("ns3::ConstantRandomVariable[Constant=1.0]"));
  ueMobility.Install (ueNodes);

  NetDeviceContainer enbLteDevs = lteHelper.InstallEnbDevice (enbNodes);
  NetDeviceContainer ueLteDevs = lteHelper.InstallUeDevice (ueNodes);

  lteHelper.Attach (ueLteDevs.Get (0), enbLteDevs.Get (0));

  InternetStackHelper internet;
  internet.Install (enbNodes);
  internet.Install (ueNodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer enbIpIface = ipv4.Assign (enbLteDevs);
  Ipv4InterfaceContainer ueIpIface = ipv4.Assign (ueLteDevs);

  uint16_t port = 5000;

  UdpServerHelper server (port);
  ApplicationContainer serverApps = server.Install (ueNodes.Get (0));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  UdpClientHelper client (ueIpIface.GetAddress (0), port);
  client.SetAttribute ("MaxPackets", UintegerValue (45));
  client.SetAttribute ("Interval", TimeValue (MilliSeconds (200)));
  client.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps = client.Install (enbNodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  Simulator::Stop (Seconds (10.0));

  lteHelper.EnablePhyTraces ();
  lteHelper.EnableMacTraces ();
  lteHelper.EnableRlcTraces ();

  AnimationInterface anim ("lte-udp-randomwalk.xml");
  anim.SetConstantPosition (enbNodes.Get(0), enbPosition.x, enbPosition.y);

  Simulator::Run ();

  Simulator::Destroy ();
  return 0;
}