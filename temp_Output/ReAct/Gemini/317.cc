#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/lte-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store.h"
#include <iostream>

using namespace ns3;

int main(int argc, char *argv[]) {

  CommandLine cmd;
  cmd.Parse(argc, argv);

  ConfigStore inputConfig;
  inputConfig.ConfigureDefaults();

  NodeContainer enbNodes;
  enbNodes.Create(1);

  NodeContainer ueNodes;
  ueNodes.Create(1);

  LteHelper lteHelper;
  lteHelper.SetEnbAntennaModelType("ns3::CosineAntennaModel");

  PointToPointEpcHelper epcHelper;
  lteHelper.SetEpcHelper(epcHelper);

  Ptr<Node> pgw = epcHelper.GetPgwNode();

  InternetStackHelper internet;
  internet.Install(ueNodes);
  internet.Install(pgw);

  NetDeviceContainer enbLteDevs = lteHelper.InstallEnbDevice(enbNodes);
  NetDeviceContainer ueLteDevs = lteHelper.InstallUeDevice(ueNodes);

  Ipv4AddressHelper ipv4h;
  ipv4h.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer ueIpIface = internet.AssignIpv4Addresses(ueLteDevs, ipv4h);

  ipv4h.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer enbIpIface = internet.AssignIpv4Addresses(enbLteDevs, ipv4h);

  Ipv4StaticRoutingHelper ipv4RoutingHelper;
  Ptr<Ipv4StaticRouting> ueStaticRouting = ipv4RoutingHelper.GetStaticRouting(ueNodes.Get(0)->GetObject<Ipv4>());
  ueStaticRouting->AddNetworkRouteTo(Ipv4Address("7.0.0.0"), Ipv4Mask("255.0.0.0"), 1);

  lteHelper.Attach(ueLteDevs, enbLteDevs.Get(0));

  // Set mobility model
  Ptr<ConstantPositionMobilityModel> enbMobility = CreateObject<ConstantPositionMobilityModel>();
  enbNodes.Get(0)->AggregateObject(enbMobility);
  enbMobility->SetPosition(Vector(0, 0, 0));

  Ptr<ConstantPositionMobilityModel> ueMobility = CreateObject<ConstantPositionMobilityModel>();
  ueNodes.Get(0)->AggregateObject(ueMobility);
  ueMobility->SetPosition(Vector(20, 20, 0));

  uint16_t dlPort = 9;

  UdpServerHelper dlPacketSinkHelper(dlPort);
  ApplicationContainer dlPacketSinkApp = dlPacketSinkHelper.Install(enbNodes.Get(0));
  dlPacketSinkApp.Start(Seconds(1.0));
  dlPacketSinkApp.Stop(Seconds(10.0));

  uint32_t packetSize = 1024;
  uint32_t numPackets = 1000;
  Time interPacketInterval = Seconds(0.01);

  UdpClientHelper dlClientHelper(enbIpIface.GetAddress(0), dlPort);
  dlClientHelper.SetAttribute("MaxPackets", UintegerValue(numPackets));
  dlClientHelper.SetAttribute("Interval", TimeValue(interPacketInterval));
  dlClientHelper.SetAttribute("PacketSize", UintegerValue(packetSize));
  ApplicationContainer dlClientApp = dlClientHelper.Install(ueNodes.Get(0));
  dlClientApp.Start(Seconds(2.0));
  dlClientApp.Stop(Seconds(10.0));

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}