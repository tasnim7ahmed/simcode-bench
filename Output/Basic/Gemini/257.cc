#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/lte-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-routing-helper.h"
#include "ns3/udp-client-server-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Config::SetDefault ("ns3::LteEnbComponentCarrier::DlEarfcn", UintegerValue (50));
  Config::SetDefault ("ns3::LteEnbComponentCarrier::UlEarfcn", UintegerValue (350));

  NodeContainer enbNodes;
  enbNodes.Create(1);
  NodeContainer ueNodes;
  ueNodes.Create(2);

  PointToPointHelper p2ph;
  p2ph.SetDeviceAttribute("DataRate", StringValue("100Gbps"));
  p2ph.SetDeviceAttribute("Mtu", UintegerValue(1500));
  p2ph.SetChannelAttribute("Delay", TimeValue(Seconds(0.001)));

  NodeContainer epcNodes;
  epcNodes.Create(1);

  NetDeviceContainer enbNetDev = p2ph.Install(epcNodes.Get(0), enbNodes.Get(0));

  Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
  Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
  epcHelper->SetPointToPoint(p2ph);
  lteHelper->SetEpcHelper(epcHelper);

  NetDeviceContainer ueNetDevs = lteHelper->InstallUeDevice(ueNodes);
  NetDeviceContainer enbNetDevs = lteHelper->InstallEnbDevice(enbNodes);

  epcHelper->AddEnb(enbNodes.Get(0));

  lteHelper->Attach(ueNetDevs.Get(0), enbNetDevs.Get(0));
  lteHelper->Attach(ueNetDevs.Get(1), enbNetDevs.Get(0));

  InternetStackHelper internet;
  internet.Install(epcNodes);
  internet.Install(ueNodes);

  Ipv4InterfaceContainer ueIpIface = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueNetDevs));
  Ipv4InterfaceContainer enbIpIface = epcHelper->AssignEnbIpv4Address(NetDeviceContainer(enbNetDevs));

  Ipv4AddressHelper ipv4h;
  ipv4h.SetBase("1.0.0.0", "255.255.255.0");
  Ipv4InterfaceContainer epcIpIface = ipv4h.Assign(enbNetDev);

  Ipv4StaticRoutingHelper ipv4RoutingHelper;
  Ptr<Node> pgw = epcNodes.Get(0);
  Ipv4StaticRouting *pgwStaticRouting = ipv4RoutingHelper.GetStaticRouting(pgw->GetObject<Ipv4>());
  pgwStaticRouting->AddNetworkRouteToInterface(Ipv4Address("7.0.0.0"), Ipv4Mask("255.0.0.0"), 1);

  UdpServerHelper server(9);
  ApplicationContainer serverApps = server.Install(ueNodes.Get(1));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  UdpClientHelper client(ueIpIface.GetAddress(1), 9);
  client.SetAttribute("MaxPackets", UintegerValue(2));
  client.SetAttribute("PacketSize", UintegerValue(512));
  client.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  ApplicationContainer clientApps = client.Install(ueNodes.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}