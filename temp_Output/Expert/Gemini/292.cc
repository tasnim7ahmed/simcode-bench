#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/lte-module.h"
#include "ns3/epc-helper.h"
#include "ns3/config-store.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);
  LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
  LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

  NodeContainer ueNodes;
  ueNodes.Create(1);
  NodeContainer enbNodes;
  enbNodes.Create(1);

  Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
  Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
  lteHelper->SetEpcHelper(epcHelper);

  Ptr<Node> pgw = epcHelper->GetPgwNode();

  InternetStackHelper internet;
  internet.Install(ueNodes);
  internet.Install(pgw);

  NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice(ueNodes);
  NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice(enbNodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer ueIpIface = ipv4.Assign(ueLteDevs);
  Ipv4InterfaceContainer pgwIpIface = ipv4.Assign(epcHelper->GetPgwP2pDevice());

  Ipv4StaticRoutingHelper ipv4RoutingHelper;
  Ptr<Ipv4StaticRouting> pgwStaticRouting = ipv4RoutingHelper.GetStaticRouting(pgw->GetObject<Ipv4>());
  pgwStaticRouting->AddNetworkRouteTo(Ipv4Address("7.0.0.0"), Ipv4Mask("255.0.0.0"), 1);

  MobilityHelper mobility;
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(enbNodes);

  mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                             "Mode", StringValue("Time"),
                             "Time", StringValue("1s"),
                             "Speed", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"),
                             "Bounds", StringValue("0 100 0 100"));

  mobility.Install(ueNodes);

  Ptr<ConstantPositionMobilityModel> enbMobility = enbNodes.Get(0)->GetObject<ConstantPositionMobilityModel>();
  enbMobility->SetPosition(Vector(50, 50, 0));

  UdpClientHelper client(pgwIpIface.GetAddress(1), 9);
  client.SetAttribute("MaxPackets", UintegerValue(1000));
  client.SetAttribute("Interval", TimeValue(MilliSeconds(500)));
  client.SetAttribute("PacketSize", UintegerValue(512));

  ApplicationContainer clientApps = client.Install(ueNodes.Get(0));
  clientApps.Start(Seconds(1));
  clientApps.Stop(Seconds(10));

  UdpServerHelper server(9);
  ApplicationContainer serverApps = server.Install(pgw);
  serverApps.Start(Seconds(1));
  serverApps.Stop(Seconds(10));

  lteHelper->Attach(ueLteDevs, enbLteDevs.Get(0));

  Simulator::Stop(Seconds(10));
  Simulator::Run();

  Simulator::Destroy();
  return 0;
}