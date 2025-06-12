#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/lte-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/config-store-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Config::SetDefault ("ns3::LteEnbRrc::SrsPeriodicity", UintegerValue(2));

  NodeContainer enbNodes;
  enbNodes.Create(1);
  NodeContainer ueNodes;
  ueNodes.Create(2);
  NodeContainer pgwNodes;
  pgwNodes.Create(1);
  NodeContainer remoteHostNodes;
  remoteHostNodes.Create(1);

  PointToPointHelper p2ph;
  p2ph.SetDeviceAttribute("DataRate", StringValue("100Gbps"));
  p2ph.SetDeviceAttribute("Mtu", UintegerValue(1500));
  p2ph.SetChannelAttribute("Delay", TimeValue(Seconds(0.010)));

  NetDeviceContainer pgwRemoteDevices;
  pgwRemoteDevices = p2ph.Install(pgwNodes.Get(0), remoteHostNodes.Get(0));

  InternetStackHelper internet;
  internet.Install(remoteHostNodes);
  internet.Install(pgwNodes);

  Ipv4AddressHelper ipv4h;
  ipv4h.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer pgwRemoteInterfaces;
  pgwRemoteInterfaces = ipv4h.Assign(pgwRemoteDevices);

  Ipv4Address remoteHostAddr = pgwRemoteInterfaces.GetAddress(1);

  LteHelper lteHelper;
  lteHelper.SetEnbDeviceAttribute("DlEarfcn", UintegerValue(5000));
  lteHelper.SetEnbDeviceAttribute("UlEarfcn", UintegerValue(20000));
  lteHelper.SetUeDeviceAttribute("DlEarfcn", UintegerValue(5000));
  lteHelper.SetUeDeviceAttribute("UlEarfcn", UintegerValue(20000));

  NetDeviceContainer enbLteDevs = lteHelper.InstallEnbDevice(enbNodes);
  NetDeviceContainer ueLteDevs = lteHelper.InstallUeDevice(ueNodes);

  internet.Install(ueNodes);

  Ipv4InterfaceContainer ueIpIface;
  ueIpIface = lteHelper.AssignUeIpv4Address(NetDeviceContainer(ueLteDevs));

  Ipv4AddressHelper ueAddressHelper;
  ueAddressHelper.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer ueInterface;
  ueInterface = ueAddressHelper.Assign(ueLteDevs);

  for (uint32_t u = 0; u < ueNodes.GetN(); ++u) {
    Ptr<Node> ue = ueNodes.Get(u);
    Ptr<Ipv4StaticRouting> ueStaticRouting = Ipv4RoutingHelper::GetRouting <Ipv4StaticRouting> (ue->GetObject<Ipv4>()->GetRoutingProtocol());
    ueStaticRouting->SetDefaultRoute(lteHelper.GetUeDefaultGatewayAddress(), 1);
  }

  Ptr<Node> pgw = pgwNodes.Get(0);
  Ptr<Ipv4StaticRouting> pgwStaticRouting = Ipv4RoutingHelper::GetRouting <Ipv4StaticRouting> (pgw->GetObject<Ipv4>()->GetRoutingProtocol());
  pgwStaticRouting->AddNetworkRouteTo(Ipv4Address("10.1.2.0"), Ipv4Mask("255.255.255.0"), lteHelper.GetUeDefaultGatewayAddress(), 1);

  lteHelper.Attach(ueLteDevs, enbLteDevs.Get(0));

  uint16_t dlPort = 2000;
  UdpClientHelper dlClient(ueIpIface.GetAddress(0), dlPort);
  dlClient.SetAttribute("MaxPackets", UintegerValue(1000));
  dlClient.SetAttribute("Interval", TimeValue(MilliSeconds(10)));
  dlClient.SetAttribute("PacketSize", UintegerValue(1472));

  ApplicationContainer dlClientApps = dlClient.Install(remoteHostNodes.Get(0));
  dlClientApps.Start(Seconds(1.0));
  dlClientApps.Stop(Seconds(10.0));

  UdpServerHelper dlPacketSink(dlPort);
  ApplicationContainer dlServerApps = dlPacketSink.Install(ueNodes.Get(0));
  dlServerApps.Start(Seconds(1.0));
  dlServerApps.Stop(Seconds(10.0));

  uint16_t ulPort = 3000;
  UdpClientHelper ulClient(remoteHostAddr, ulPort);
  ulClient.SetAttribute("MaxPackets", UintegerValue(1000));
  ulClient.SetAttribute("Interval", TimeValue(MilliSeconds(10)));
  ulClient.SetAttribute("PacketSize", UintegerValue(1472));

  ApplicationContainer ulClientApps = ulClient.Install(ueNodes.Get(1));
  ulClientApps.Start(Seconds(1.0));
  ulClientApps.Stop(Seconds(10.0));

  UdpServerHelper ulPacketSink(ulPort);
  ApplicationContainer ulServerApps = ulPacketSink.Install(remoteHostNodes.Get(0));
  ulServerApps.Start(Seconds(1.0));
  ulServerApps.Stop(Seconds(10.0));

  AnimationInterface anim("lte-netanim.xml");
  anim.SetConstantPosition(remoteHostNodes.Get(0), 0.0, 20.0);
  anim.SetConstantPosition(pgwNodes.Get(0), 20.0, 20.0);
  anim.SetConstantPosition(enbNodes.Get(0), 40.0, 20.0);
  anim.SetConstantPosition(ueNodes.Get(0), 60.0, 10.0);
  anim.SetConstantPosition(ueNodes.Get(1), 60.0, 30.0);

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}