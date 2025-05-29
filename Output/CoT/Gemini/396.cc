#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/lte-module.h"
#include "ns3/udp-echo-helper.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/mobility-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {

  CommandLine cmd;
  cmd.Parse(argc, argv);

  Config::SetDefault ("ns3::LteUePhy::EnableUplinkPowerControl", BooleanValue (false));

  NodeContainer enbNodes;
  enbNodes.Create(1);
  NodeContainer ueNodes;
  ueNodes.Create(1);

  LteHelper lteHelper;
  lteHelper.SetEnbDeviceAttribute ("DlEarfcn", UintegerValue (100));
  lteHelper.SetEnbDeviceAttribute ("UlEarfcn", UintegerValue (18100));

  PointToPointEpcHelper epcHelper;
  lteHelper.SetEpcHelper (&epcHelper);

  Ptr<Node> enb = enbNodes.Get(0);
  Ptr<Node> ue = ueNodes.Get(0);

  MobilityHelper mobility;
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (enbNodes);
  mobility.Install (ueNodes);
  Ptr<ConstantPositionMobilityModel> enbMobility = enb->GetObject<ConstantPositionMobilityModel> ();
  Ptr<ConstantPositionMobilityModel> ueMobility = ue->GetObject<ConstantPositionMobilityModel> ();
  enbMobility->SetPosition (Vector (0.0, 0.0, 0.0));
  ueMobility->SetPosition (Vector (5.0, 0.0, 0.0));


  NetDeviceContainer enbLteDevs = lteHelper.InstallEnbDevice (enbNodes);
  NetDeviceContainer ueLteDevs = lteHelper.InstallUeDevice (ueNodes);

  InternetStackHelper internet;
  internet.Install (enbNodes);
  internet.Install (ueNodes);

  Ipv4InterfaceContainer enbIpIface = epcHelper.AssignIpv4Address (NetDeviceContainer (enbLteDevs));
  Ipv4InterfaceContainer ueIpIface = epcHelper.AssignIpv4Address (NetDeviceContainer (ueLteDevs));

  lteHelper.Attach (ueLteDevs.Get(0), enbLteDevs.Get(0));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  uint16_t port = 12345;

  UdpEchoServerHelper echoServer (port);
  ApplicationContainer serverApps = echoServer.Install (ueNodes.Get (0));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  UdpClientHelper echoClient (ueIpIface.GetAddress (0), port);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (1000));
  echoClient.SetAttribute ("Interval", TimeValue (MilliSeconds (20)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (10));
  ApplicationContainer clientApps = echoClient.Install (enbNodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}