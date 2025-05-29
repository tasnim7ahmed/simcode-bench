#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/lte-module.h"
#include "ns3/epc-helper.h"
#include "ns3/netanim-module.h"
#include "ns3/config-store.h"
#include "ns3/nr-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("5gNrUeGnbExample");

int main (int argc, char *argv[])
{
  uint16_t numberOfUes = 1;
  double simTime = 10;
  double interPacketInterval = 0.5;

  CommandLine cmd;
  cmd.AddValue ("numberOfUes", "Number of UEs in the simulation", numberOfUes);
  cmd.AddValue ("simTime", "Simulation time in seconds", simTime);
  cmd.AddValue ("interPacketInterval", "Inter packet interval in seconds", interPacketInterval);
  cmd.Parse (argc, argv);

  Config::SetDefault ("ns3::UdpClient::Interval", TimeValue (Seconds (interPacketInterval)));
  Config::SetDefault ("ns3::UdpClient::MaxPackets", UintegerValue (1000000));

  NodeContainer enbNodes;
  enbNodes.Create (1);
  NodeContainer ueNodes;
  ueNodes.Create (numberOfUes);

  Ptr<Node> enbNode = enbNodes.Get (0);

  // Create the EPC helper
  Ptr<LteHelper> lteHelper = CreateObject<LteHelper> ();
  Ptr<EpcHelper> epcHelper = CreateObject<EpcHelper> ();
  lteHelper->SetEpcHelper (epcHelper);

  // Create 5G NR Helper
  Ptr<NrHelper> nrHelper = CreateObject<NrHelper> ();

  // Set up the network topology
  PointToPointHelper p2pHelper;
  p2pHelper.SetDeviceAttribute ("DataRate", DataRateValue (DataRate ("100Gb/s")));
  p2pHelper.SetDeviceAttribute ("Mtu", UintegerValue (1500));
  p2pHelper.SetChannelAttribute ("Delay", TimeValue (Seconds (0.010)));

  NodeContainer sgwPgwNodes;
  sgwPgwNodes.Create (1);

  InternetStackHelper internet;
  internet.Install (sgwPgwNodes);

  NetDeviceContainer sgwPgwDevices;
  sgwPgwDevices = p2pHelper.Install (enbNode, sgwPgwNodes.Get (0));

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer sgwPgwInterfaces;
  sgwPgwInterfaces = address.Assign (sgwPgwDevices);

  Ptr<Node> pgw = sgwPgwNodes.Get (0);
  Ipv4StaticRoutingHelper ipv4RoutingHelper;
  Ptr<Ipv4StaticRouting> pgwStaticRouting = ipv4RoutingHelper.GetStaticRouting (pgw->GetObject<Ipv4> ());
  pgwStaticRouting->AddNetworkRouteTo (Ipv4Address ("7.0.0.0"), Ipv4Mask ("255.0.0.0"), 1);

  // Install Mobility Model
  MobilityHelper enbMobility;
  enbMobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  enbMobility.Install (enbNodes);

  MobilityHelper ueMobility;
  ueMobility.SetPositionAllocator ("ns3::RandomDiscPositionAllocator",
                                     "X", DoubleValue (0.0),
                                     "Y", DoubleValue (0.0),
                                     "Rho", StringValue ("ns3::UniformRandomVariable[Min=100|Max=500]"));
  ueMobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                                  "Mode", StringValue ("Time"),
                                  "Time", StringValue ("1s"),
                                  "Speed", StringValue ("ns3::ConstantRandomVariable[Constant=1.0]"),
                                  "Bounds", RectangleValue (Rectangle (-1000, -1000, 1000, 1000)));
  ueMobility.Install (ueNodes);

  // Install LTE Devices to the nodes
  NetDeviceContainer enbLteDevs;
  enbLteDevs = lteHelper->InstallEnbDevice (enbNodes);

  NetDeviceContainer ueLteDevs;
  ueLteDevs = lteHelper->InstallUeDevice (ueNodes);

  // Attach the UEs to the closest eNB
  lteHelper->Attach (ueLteDevs, enbLteDevs.Get (0));

  // Set active protocol to UDP
  enum EpsBearer::Qci q = EpsBearer::GBR_QCI_DEFAULT;
  EpsBearer bearer (q);
  lteHelper->ActivateDataRadioBearer (ueLteDevs, bearer);

  // Install the IP stack on the UEs
  InternetStackHelper internetUe;
  internetUe.Install (ueNodes);
  Ipv4InterfaceContainer ueIpIface;
  ueIpIface = epcHelper->AssignUeIpv4Address (NetDeviceContainer (ueLteDevs));

  // Set the default gateway for the UE
  for (uint32_t u = 0; u < ueNodes.GetN (); ++u)
    {
      Ptr<Node> ue = ueNodes.Get (u);
      Ptr<Ipv4StaticRouting> ueStaticRouting = ipv4RoutingHelper.GetStaticRouting (ue->GetObject<Ipv4> ());
      ueStaticRouting->SetDefaultRoute (epcHelper->GetUeDefaultGatewayAddress (), 1);
    }

  // Install and start applications on the UEs
  uint16_t dlPort = 20000;
  for (uint32_t u = 0; u < ueNodes.GetN (); ++u)
    {
      Ptr<Node> ue = ueNodes.Get (u);

      UdpClientHelper dlClient (ueIpIface.GetAddress (u), dlPort);
      dlClient.SetAttribute ("PacketSize", UintegerValue (512));
      ApplicationContainer dlClientApps = dlClient.Install (enbNodes.Get (0));
      dlClientApps.Start (Seconds (2));
      dlClientApps.Stop (Seconds (simTime));

      UdpServerHelper dlPacketSink (dlPort);
      ApplicationContainer dlServerApps = dlPacketSink.Install (ue);
      dlServerApps.Start (Seconds (1));
      dlServerApps.Stop (Seconds (simTime));

      dlPort++;
    }

  // Install and start applications on the UEs
  uint16_t ulPort = 10000;
  for (uint32_t u = 0; u < ueNodes.GetN (); ++u)
    {
      Ptr<Node> ue = ueNodes.Get (u);
      UdpClientHelper ulClient (sgwPgwInterfaces.GetAddress (0), ulPort);
      ulClient.SetAttribute ("PacketSize", UintegerValue (512));
      ApplicationContainer ulClientApps = ulClient.Install (ue);
      ulClientApps.Start (Seconds (2));
      ulClientApps.Stop (Seconds (simTime));

      UdpServerHelper ulPacketSink (ulPort);
      ApplicationContainer ulServerApps = ulPacketSink.Install (enbNodes.Get(0));
      ulServerApps.Start (Seconds (1));
      ulServerApps.Stop (Seconds (simTime));

      ulPort++;
    }

  Simulator::Stop (Seconds (simTime));

  AnimationInterface anim ("5g-nr-ue-gnb.xml");
  anim.SetConstantPosition (enbNodes.Get(0), 10, 10);

  Simulator::Run ();

  Simulator::Destroy ();

  return 0;
}