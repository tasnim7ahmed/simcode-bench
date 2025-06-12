#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/lte-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/config-store-module.h"
#include "ns3/netanim-module.h"
#include <iostream>

using namespace ns3;

int main(int argc, char *argv[]) {
  Config::SetDefault ("ns3::LteEnbRrc::SrsPeriodicity", UintegerValue (80));
  uint32_t numberOfEnbs = 2;
  uint32_t numberOfUes = 1;

  CommandLine cmd;
  cmd.Parse (argc, argv);

  NodeContainer enbNodes;
  enbNodes.Create (numberOfEnbs);

  NodeContainer ueNodes;
  ueNodes.Create (numberOfUes);

  MobilityHelper enbMobility;
  enbMobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");

  for (uint32_t i = 0; i < numberOfEnbs; ++i) {
    Ptr<Node> enbNode = enbNodes.Get (i);
    enbMobility.Install (enbNode);
    Ptr<ConstantPositionMobilityModel> enbMobModel = enbNode->GetObject<ConstantPositionMobilityModel> ();
    enbMobModel->SetPosition (Vector (i * 500.0, 0, 0));
  }

  MobilityHelper ueMobility;
  ueMobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                                "Bounds", RectangleValue (Rectangle (0, 500, 0, 10)));

  ueMobility.Install (ueNodes);
  Ptr<Node> ueNode = ueNodes.Get (0);
  Ptr<RandomWalk2dMobilityModel> ueMobModel = ueNode->GetObject<RandomWalk2dMobilityModel> ();
  ueMobModel->SetAttribute ("Speed", StringValue ("ns3::ConstantRandomVariable[Constant=10.0]"));

  LteHelper lteHelper;
  lteHelper.SetEnbDeviceAttribute ("DlEarfcn", UintegerValue (500));
  lteHelper.SetEnbDeviceAttribute ("UlEarfcn", UintegerValue (18500));

  NetDeviceContainer enbLteDevs = lteHelper.InstallEnbDevice (enbNodes);
  NetDeviceContainer ueLteDevs = lteHelper.InstallUeDevice (ueNodes);

  lteHelper.Attach (ueLteDevs.Get (0), enbLteDevs.Get (0));

  InternetStackHelper internet;
  internet.Install (enbNodes);
  internet.Install (ueNodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer ueIpIface = ipv4.Assign (ueLteDevs);
  Ipv4InterfaceContainer enbIpIface = ipv4.Assign (enbLteDevs);

  uint16_t dlPort = 5000;

  UdpServerHelper dlServer (dlPort);
  ApplicationContainer dlServerApps = dlServer.Install (enbNodes.Get (0));
  dlServerApps.Start (Seconds (1.0));
  dlServerApps.Stop (Seconds (10.0));

  UdpClientHelper dlClient (enbIpIface.GetAddress (0), dlPort);
  dlClient.SetAttribute ("MaxPackets", UintegerValue (1000000));
  dlClient.SetAttribute ("Interval", TimeValue (Time ("0.001")));
  dlClient.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer dlClientApps = dlClient.Install (ueNodes.Get (0));
  dlClientApps.Start (Seconds (2.0));
  dlClientApps.Stop (Seconds (10.0));

  lteHelper.HandoverRequest (Seconds (5.0), ueLteDevs.Get (0), enbLteDevs.Get (0), enbLteDevs.Get (1));

  Simulator::Stop (Seconds (10.0));

  AnimationInterface anim ("lte-handover.xml");
  anim.SetConstantPosition (enbNodes.Get (0), 0, 0);
  anim.SetConstantPosition (enbNodes.Get (1), 500, 0);

  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}