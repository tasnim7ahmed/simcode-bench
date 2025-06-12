#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/lte-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/config-store-module.h"
#include "ns3/netanim-module.h"

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
  lteHelper.SetEnbAntennaModelType ("ns3::CosineAntennaModel");
  lteHelper.SetUeTransmitterAntennaModelType ("ns3::IsotropicAntennaModel");

  PointToPointEpcHelper epcHelper;
  lteHelper.SetEpcHelper (&epcHelper);

  NetDeviceContainer enbDevs;
  enbDevs = lteHelper.InstallEnbDevice (enbNodes);
  NetDeviceContainer ueDevs;
  ueDevs = lteHelper.InstallUeDevice (ueNodes);

  InternetStackHelper internet;
  internet.Install (ueNodes);
  internet.Install (enbNodes);
  epcHelper.InstallEpcDevice (enbNodes);

  Ipv4AddressHelper ip;
  ip.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer ueIpIface = ip.Assign (ueDevs);
  Ipv4InterfaceContainer enbIpIface = ip.Assign (enbDevs);

  lteHelper.Attach (ueDevs, enbDevs.Get(0));

  Ptr<Node> ue = ueNodes.Get(0);
  Ptr<Node> enb = enbNodes.Get(0);

  MobilityHelper mobility;
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (enbNodes);

  mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                             "Mode", StringValue ("Time"),
                             "Time", StringValue ("1s"),
                             "Speed", StringValue ("ns3::ConstantRandomVariable[Constant=1.0]"),
                             "Bounds", StringValue ("0 100 0 100"));
  mobility.Install (ueNodes);

  uint16_t dlPort = 9;
  UdpServerHelper dlPacketSinkHelper (dlPort);
  ApplicationContainer dlPacketSinkApp = dlPacketSinkHelper.Install (ueNodes);
  dlPacketSinkApp.Start (Seconds (0.0));
  dlPacketSinkApp.Stop (Seconds (10.0));

  uint16_t ulPort = 9;
  UdpClientHelper ulPacketSourceHelper (ueIpIface.GetAddress (0), ulPort);
  ulPacketSourceHelper.SetAttribute ("MaxPackets", UintegerValue (1000000));
  ulPacketSourceHelper.SetAttribute ("Interval", TimeValue (MilliSeconds (100)));
  ulPacketSourceHelper.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer ulPacketSourceApps = ulPacketSourceHelper.Install (enbNodes);
  ulPacketSourceApps.Start (Seconds (1.0));
  ulPacketSourceApps.Stop (Seconds (10.0));


  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}