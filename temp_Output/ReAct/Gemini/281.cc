#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/udp-client-server-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Config::SetDefault ("ns3::UdpClient::Interval", TimeValue (Seconds (1.)));
  Config::SetDefault ("ns3::UdpClient::MaxPackets", UintegerValue (10));

  NodeContainer enbNodes;
  enbNodes.Create(1);
  NodeContainer ueNodes;
  ueNodes.Create(3);

  LteHelper lteHelper;
  lteHelper.SetEnbDeviceAttribute ("DlEarfcn", UintegerValue (100));
  lteHelper.SetEnbDeviceAttribute ("UlEarfcn", UintegerValue (18100));

  PointToPointEpcHelper epcHelper;
  lteHelper.SetEpcHelper (&epcHelper);

  NetDeviceContainer enbLteDevs = lteHelper.InstallEnbDevice (enbNodes);
  NetDeviceContainer ueLteDevs = lteHelper.InstallUeDevice (ueNodes);

  InternetStackHelper internet;
  internet.Install (enbNodes);
  internet.Install (ueNodes);

  Ipv4InterfaceContainer enbIpIface = epcHelper.AssignIpv4Address (NetDeviceContainer (enbLteDevs.Get (0)));
  Ipv4InterfaceContainer ueIpIface = epcHelper.AssignIpv4Address (ueLteDevs);

  Ipv4AddressHelper ipv4h;
  ipv4h.SetBase ("1.0.0.0", "255.0.0.0");
  Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign (NetDeviceContainer (enbLteDevs.Get (0)));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  uint16_t dlPort = 9;
  UdpServerHelper dlPacketSinkHelper (dlPort);
  ApplicationContainer dlPacketSinkApp = dlPacketSinkHelper.Install (enbNodes.Get (0));
  dlPacketSinkApp.Start (Seconds (0));
  dlPacketSinkApp.Stop (Seconds (10));

  UdpClientHelper ulPacketSourceHelper (enbIpIface.GetAddress (0), dlPort);
  ulPacketSourceHelper.SetAttribute ("MaxPackets", UintegerValue (10));
  ulPacketSourceHelper.SetAttribute ("Interval", TimeValue (Seconds (1.)));
  ApplicationContainer ulPacketSourceApps;
  for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
    {
      ulPacketSourceApps.Add (ulPacketSourceHelper.Install (ueNodes.Get (i)));
    }
  ulPacketSourceApps.Start (Seconds (1));
  ulPacketSourceApps.Stop (Seconds (10));

  Ptr<RandomWalk2dMobilityModel> randomWalk;
  MobilityHelper mobility;
  mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                                 "Mode", StringValue ("Time"),
                                 "Time", StringValue ("1s"),
                                 "Speed", StringValue ("1m/s"),
                                 "Bounds", RectangleValue (Rectangle (0, 50, 0, 50)));
  mobility.Install (ueNodes);

  Ptr<ConstantPositionMobilityModel> enbMobility = CreateObject<ConstantPositionMobilityModel> ();
  enbNodes.Get (0)->SetMobility (enbMobility);
  enbMobility->SetPosition (Vector (25, 25, 0));

  lteHelper.Attach (ueLteDevs, enbLteDevs.Get (0));

  Simulator::Stop (Seconds (10));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}