#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/random-walk-2d-mobility-model.h"
#include "ns3/udp-client-server-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Config::SetDefault ("ns3::LteEnbRrc::SrsPeriodicity", UintegerValue (160));

  NodeContainer enbNodes;
  enbNodes.Create(1);

  NodeContainer ueNodes;
  ueNodes.Create(3);

  LteHelper lteHelper;
  lteHelper.SetEnbDeviceAttribute("DlEarfcn", UintegerValue(200));
  lteHelper.SetEnbDeviceAttribute("UlEarfcn", UintegerValue(18200));

  PointToPointEpcHelper epcHelper;
  lteHelper.SetEpcHelper(epcHelper);

  Ptr<Node> pgw = epcHelper.GetPgwNode();
  InternetStackHelper internet;
  internet.Install(pgw);

  NetDeviceContainer enbLteDevs;
  enbLteDevs = lteHelper.InstallEnbDevice(enbNodes);

  NetDeviceContainer ueLteDevs;
  ueLteDevs = lteHelper.InstallUeDevice(ueNodes);

  internet.Install(ueNodes);

  Ipv4AddressHelper ipv4h;
  ipv4h.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer ueIpIface = ipv4h.Assign(ueLteDevs);
  Ipv4InterfaceContainer enbIpIface = ipv4h.Assign(enbLteDevs);

  Ipv4StaticRoutingHelper ipv4RoutingHelper;
  Ptr<Ipv4StaticRouting> pgwStaticRouting = ipv4RoutingHelper.GetStaticRouting(pgw->GetObject<Ipv4>());
  pgwStaticRouting->AddNetworkRouteTo(Ipv4Address("7.0.0.0"), Ipv4Mask("255.0.0.0"), 1);

  for (uint32_t u = 0; u < ueNodes.GetN(); ++u) {
    Ptr<Node> ue = ueNodes.Get(u);
    Ptr<Ipv4StaticRouting> ueStaticRouting = ipv4RoutingHelper.GetStaticRouting(ue->GetObject<Ipv4>());
    ueStaticRouting->SetDefaultRoute(epcHelper.GetPgwNode()->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal(), 1);
  }

  MobilityHelper mobility;
  mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                            "Mode", StringValue("Time"),
                            "Time", StringValue("2s"),
                            "Speed", StringValue("1.0"),
                            "Bounds", RectangleValue(Rectangle(0, 50, 0, 50)));
  mobility.Install(ueNodes);

  Ptr<ConstantPositionMobilityModel> enbMobility = CreateObject<ConstantPositionMobilityModel>();
  enbNodes.Get(0)->AggregateObject(enbMobility);
  Vector enbPosition(25, 25, 0);
  enbMobility->SetPosition(enbPosition);

  lteHelper.Attach(ueLteDevs, enbLteDevs.Get(0));

  uint16_t dlPort = 9;
  UdpServerHelper dlPacketSinkHelper(dlPort);
  ApplicationContainer dlPacketSinkApp = dlPacketSinkHelper.Install(enbNodes.Get(0));
  dlPacketSinkApp.Start(Seconds(0.0));
  dlPacketSinkApp.Stop(Seconds(10.0));

  UdpClientHelper ulPacketSourceHelper(enbIpIface.GetAddress(0), dlPort);
  ulPacketSourceHelper.SetAttribute("MaxPackets", UintegerValue(10));
  ulPacketSourceHelper.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  ulPacketSourceHelper.SetAttribute("PacketSize", UintegerValue(1472));

  ApplicationContainer ulPacketSourceApps;
  for (uint32_t i = 0; i < ueNodes.GetN(); ++i) {
      ulPacketSourceApps.Add(ulPacketSourceHelper.Install(ueNodes.Get(i)));
  }

  ulPacketSourceApps.Start(Seconds(1.0));
  ulPacketSourceApps.Stop(Seconds(10.0));

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}