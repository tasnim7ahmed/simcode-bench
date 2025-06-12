#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/lte-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/config-store-module.h"
#include "ns3/random-variable-stream.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Config::SetDefault ("ns3::LteUeRrc::SrsPeriodicity", UintegerValue (80));

  NodeContainer enbNodes;
  enbNodes.Create(1);
  NodeContainer ueNodes;
  ueNodes.Create(1);

  LteHelper lteHelper;
  lteHelper.SetEnbAntennaModelType ("ns3::CosineAntennaModel");

  PointToPointEpcHelper epcHelper;
  lteHelper.SetEpcHelper (&epcHelper);

  NetDeviceContainer enbLteDevs = lteHelper.InstallEnbDevice (enbNodes);
  NetDeviceContainer ueLteDevs = lteHelper.InstallUeDevice (ueNodes);

  InternetStackHelper internet;
  epcHelper.InstallInternetStack (NodeContainer::GetGlobal ());
  internet.Install (ueNodes);

  Ipv4AddressHelper ipv4Helper;
  ipv4Helper.SetBase ("1.0.0.0", "255.0.0.0");
  Ipv4InterfaceContainer ueIpIface = ipv4Helper.Assign (ueLteDevs);

  Ipv4InterfaceContainer epcIpIface = epcHelper.AssignUeIpv4Address (NetDeviceContainer::GetGlobal ());

  lteHelper.Attach (ueLteDevs, enbLteDevs.Get(0));

  lteHelper.ActivateDedicatedBearer (ueLteDevs, EpsBearer (EpsBearer::Qci::GBR_CONV_VOICE));

  Ptr<Node> ue = ueNodes.Get(0);
  Ptr<Node> enb = enbNodes.Get(0);

  MobilityHelper enbMobility;
  Ptr<ListPositionAllocator> enbPositionAlloc = CreateObject<ListPositionAllocator> ();
  enbPositionAlloc->Add (Vector (0.0, 0.0, 0.0));
  enbMobility.SetPositionAllocator (enbPositionAlloc);
  enbMobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  enbMobility.Install (enbNodes);

  MobilityHelper ueMobility;
  ObjectFactory pos;
  pos.SetTypeId ("ns3::RandomBoxPositionAllocator");
  pos.Set ("X", StringValue ("ns3::UniformRandomVariable[Min=1.0|Max=10.0]"));
  pos.Set ("Y", StringValue ("ns3::UniformRandomVariable[Min=1.0|Max=10.0]"));
  pos.Set ("Z", StringValue ("ns3::ConstantRandomVariable[Constant=0.0]"));
  Ptr<PositionAllocator> taPositionAlloc = pos.Create ()->GetObject<PositionAllocator> ();
  ueMobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                                "Mode", StringValue ("Time"),
                                "Time", StringValue ("1s"),
                                "Speed", StringValue ("ns3::ConstantRandomVariable[Constant=1.0]"),
                                "Bounds", StringValue ("10x10"));
  ueMobility.SetPositionAllocator (taPositionAlloc);
  ueMobility.Install (ueNodes);

  uint16_t dlPort = 20000;
  UdpClientHelper dlClient (epcIpIface.GetAddress (0), dlPort);
  dlClient.SetAttribute ("MaxPackets", UintegerValue (1000000));
  dlClient.SetAttribute ("Interval", TimeValue (MilliSeconds (1)));
  dlClient.SetAttribute ("PacketSize", UintegerValue (1472));
  ApplicationContainer dlClientApps = dlClient.Install (ueNodes.Get(0));
  dlClientApps.Start (Seconds (1.0));
  dlClientApps.Stop (Seconds (10.0));

  UdpServerHelper dlPacketSink (dlPort);
  ApplicationContainer dlPacketSinkApps = dlPacketSink.Install (enbNodes.Get(0));
  dlPacketSinkApps.Start (Seconds (0.0));
  dlPacketSinkApps.Stop (Seconds (10.0));

  Simulator::Stop (Seconds (10));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}