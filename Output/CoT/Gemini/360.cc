#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/lte-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Config::SetDefault ("ns3::LteEnbRrc::SrsPeriodicity", UintegerValue (80));

  NodeContainer enbNodes;
  enbNodes.Create(1);
  NodeContainer ueNodes;
  ueNodes.Create(1);

  LteHelper lteHelper;
  lteHelper.SetEnbAntennaModelType("ns3::CosineAntennaModel");
  lteHelper.SetUeAntennaModelType("ns3::CosineAntennaModel");

  PointToPointEpcHelper epcHelper;
  lteHelper.SetEpcHelper( &epcHelper );

  Ptr<Node> pgw = epcHelper.GetPgwNode ();

  InternetStackHelper internet;
  internet.Install (pgw);

  NetDeviceContainer enbLteDevs;
  enbLteDevs = lteHelper.InstallEnbDevice (enbNodes);

  NetDeviceContainer ueLteDevs;
  ueLteDevs = lteHelper.InstallUeDevice (ueNodes);

  internet.Install (ueNodes);

  Ipv4AddressHelper ipv4Helper;
  ipv4Helper.SetBase ("10.0.0.0", "255.255.255.0");
  Ipv4InterfaceContainer ueIpIface = ipv4Helper.Assign (ueLteDevs);
  Ipv4InterfaceContainer enbIpIface = ipv4Helper.Assign (enbLteDevs);
  internet.Assign (NetDeviceContainer (pgw->GetDevice (0)));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  Ptr<Node> enb = enbNodes.Get(0);
  Ptr<Node> ue = ueNodes.Get(0);

  MobilityHelper mobility;
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (enbNodes);
  mobility.Install (ueNodes);

  Ptr<ConstantPositionMobilityModel> enbMob = enb->GetObject<ConstantPositionMobilityModel> ();
  Ptr<ConstantPositionMobilityModel> ueMob = ue->GetObject<ConstantPositionMobilityModel> ();
  enbMob->SetPosition (Vector (0, 0, 0));
  ueMob->SetPosition (Vector (5, 0, 0));

  lteHelper.Attach (ueLteDevs.Get (0), enbLteDevs.Get (0));

  uint16_t port = 9;

  UdpEchoServerHelper echoServer (port);
  ApplicationContainer serverApps = echoServer.Install (enbNodes.Get (0));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  UdpEchoClientHelper echoClient (enbIpIface.GetAddress (0), port);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (100));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (0.1)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientApps = echoClient.Install (ueNodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}