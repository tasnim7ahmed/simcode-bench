#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/lte-module.h"
#include "ns3/config-store-module.h"
#include "ns3/mobility-module.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Config::SetDefault("ns3::LteUePhy::EnableUplinkPowerControl", BooleanValue(false));

  NodeContainer ueNodes;
  ueNodes.Create(1);
  NodeContainer enbNodes;
  enbNodes.Create(1);
  NodeContainer epcNodes;
  epcNodes.Create(1);

  Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
  Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
  lteHelper->SetEpcHelper(epcHelper);

  Ptr<Node> pgw = epcHelper->GetPgwNode();
  PointToPointHelper p2ph;
  p2ph.SetDeviceAttribute("DataRate", DataRateValue(DataRate("100Gb/s")));
  p2ph.SetDeviceAttribute("Mtu", UintegerValue(1500));
  p2ph.SetChannelAttribute("Delay", TimeValue(Seconds(0.010)));
  NetDeviceContainer epcDevices;
  epcDevices = p2ph.Install(pgw, epcNodes.Get(0));

  InternetStackHelper internet;
  internet.Install(ueNodes);
  internet.Install(epcNodes);

  Ipv4AddressHelper ipv4h;
  ipv4h.SetBase("1.0.0.0", "255.0.0.0");
  Ipv4InterfaceContainer internetInterfaces = ipv4h.Assign(epcDevices);
  Ipv4Address internetAddress = internetInterfaces.GetAddress(1);

  Ipv4StaticRoutingHelper ipv4RoutingHelper;
  Ptr<Ipv4StaticRouting> remoteRouter = ipv4RoutingHelper.GetStaticRouting(epcNodes.Get(0)->GetObject<Ipv4>());
  remoteRouter->AddNetworkRouteTo(Ipv4Address("7.0.0.0"), Ipv4Mask("255.0.0.0"), 1);

  MobilityHelper mobility;
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(enbNodes);
  mobility.Install(ueNodes);

  NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice(enbNodes);
  NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice(ueNodes);

  lteHelper->Attach(ueLteDevs.Get(0), enbLteDevs.Get(0));

  Ipv4InterfaceContainer ueIpIface = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueLteDevs));

  Ptr<Node> ue = ueNodes.Get(0);
  Ptr<Ipv4> ueIpv4 = ue->GetObject<Ipv4>();
  Ipv4Address ueAddress = ueIpv4->GetAddress(1, 0).GetLocal();

  uint16_t port = 9;
  UdpServerHelper server(port);
  ApplicationContainer serverApps = server.Install(ueNodes.Get(0));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  UdpClientHelper client(ueAddress, port);
  client.SetAttribute("MaxPackets", UintegerValue(1000));
  client.SetAttribute("Interval", TimeValue(Seconds(0.01)));
  client.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientApps = client.Install(enbNodes.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}