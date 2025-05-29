#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/lte-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/config-store.h"
#include "ns3/netanim-module.h"
#include "ns3/point-to-point-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  Config::SetDefault ("ns3::LteUePhy::EnableUplinkPowerControl", BooleanValue (false));

  CommandLine cmd;
  cmd.Parse (argc, argv);

  NodeContainer enbNodes;
  enbNodes.Create (1);
  NodeContainer ueNodes;
  ueNodes.Create (1);

  LteHelper lteHelper;
  lteHelper.SetEnbAntennaModelType ("ns3::CosineAntennaModel");
  lteHelper.SetAttribute ("PathlossModel", StringValue ("ns3::LogDistancePropagationLossModel"));

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("1Gbps"));
  p2p.SetDeviceAttribute ("Mtu", StringValue ("1500"));
  p2p.SetChannelAttribute ("Delay", StringValue ("1ms"));

  NetDeviceContainer enbNetDev = lteHelper.InstallEnbDevice (enbNodes);
  NetDeviceContainer ueNetDev = lteHelper.InstallUeDevice (ueNodes);

  MobilityHelper mobility;
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (enbNodes);
  Ptr<ConstantPositionMobilityModel> enbMobility = enbNodes.Get(0)->GetObject<ConstantPositionMobilityModel>();
  enbMobility->SetPosition(Vector(0.0, 0.0, 0.0));

  mobility.SetMobilityModel ("ns3::RandomBoxPositionAllocator",
                                 "X", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                 "Y", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                 "Z", StringValue ("ns3::ConstantRandomVariable[Constant=0.0]"));
  mobility.SetVelocityModel ("ns3::UniformRandomVariable[Min=-1.0|Max=1.0]");
  mobility.Install (ueNodes);

  InternetStackHelper internet;
  internet.Install (ueNodes);
  internet.Install (enbNodes);

  Ipv4InterfaceContainer ueIpIface;
  ueIpIface = internet.AssignIpv4Addresses (ueNetDev, Ipv4Address ("10.1.1.0"), Ipv4Mask ("255.255.255.0"));
  Ipv4InterfaceContainer enbIpIface;
  enbIpIface = internet.AssignIpv4Addresses (enbNetDev, Ipv4Address ("10.1.2.0"), Ipv4Mask ("255.255.255.0"));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  uint16_t dlPort = 5000;

  UdpServerHelper dlServer (dlPort);
  ApplicationContainer dlServerApps = dlServer.Install (ueNodes.Get (0));
  dlServerApps.Start (Seconds (0.1));
  dlServerApps.Stop (Seconds (10.0));

  uint32_t packetSize = 1024;
  Time interPacketInterval = MilliSeconds (200);

  UdpClientHelper dlClient (ueIpIface.GetAddress (0), dlPort);
  dlClient.SetAttribute ("MaxPackets", UintegerValue (50));
  dlClient.SetAttribute ("Interval", TimeValue (interPacketInterval));
  dlClient.SetAttribute ("PacketSize", UintegerValue (packetSize));

  ApplicationContainer dlClientApps = dlClient.Install (enbNodes.Get (0));
  dlClientApps.Start (Seconds (1.0));
  dlClientApps.Stop (Seconds (10.0));

  lteHelper.Attach (ueNetDev.Get (0), enbNetDev.Get (0));

  Simulator::Stop (Seconds (10.0));

  lteHelper.EnablePhyTraces ();
  lteHelper.EnableMacTraces ();
  lteHelper.EnableRlcTraces ();

  AnimationInterface anim ("lte-random-mobility.xml");
  anim.SetConstantPosition (enbNodes.Get (0), 0.0, 0.0);

  Simulator::Run ();

  Simulator::Destroy ();
  return 0;
}