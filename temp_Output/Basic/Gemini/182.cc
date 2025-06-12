#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/lte-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/epc-helper.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {

  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);
  LogComponentEnable ("LteEnbNetDevice", LOG_LEVEL_INFO);
  LogComponentEnable ("LteUeNetDevice", LOG_LEVEL_INFO);
  LogComponentEnable ("EpcSgwPgwApplication", LOG_LEVEL_ALL);

  Config::SetDefault ("ns3::LteEnbRrc::SrsPeriodicity", UintegerValue (160));

  NodeContainer enbNodes;
  enbNodes.Create(1);

  NodeContainer ueNodes;
  ueNodes.Create(2);

  Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
  Ptr<EpcHelper> epcHelper = CreateObject<EpcHelper>();
  lteHelper->SetEpcHelper(epcHelper);

  PointToPointHelper p2ph;
  p2ph.SetDeviceAttribute("DataRate", DataRateValue(DataRate("100Gb/s")));
  p2ph.SetDeviceAttribute("Mtu", UintegerValue(1500));
  p2ph.SetChannelAttribute("Delay", TimeValue(MilliSeconds(1)));

  NodeContainer epcNodes;
  epcNodes.Add(epcHelper->GetPgwNode());

  NetDeviceContainer enbDevs = lteHelper->InstallEnbDevice(enbNodes);
  NetDeviceContainer ueDevs = lteHelper->InstallUeDevice(ueNodes);

  InternetStackHelper internet;
  internet.Install(ueNodes);
  internet.Install(epcNodes);

  Ipv4AddressHelper ipv4h;
  ipv4h.SetBase("1.0.0.0", "255.255.255.0");
  Ipv4InterfaceContainer ueIpIface = ipv4h.Assign(ueDevs);

  ipv4h.SetBase("1.0.1.0", "255.255.255.0");
  Ipv4InterfaceContainer epcIpIface = ipv4h.Assign(epcHelper->GetPgwNode()->GetDevice(0));

  Ipv4StaticRoutingHelper ipv4RoutingHelper;
  Ptr<Ipv4StaticRouting> enbStaticRouting = ipv4RoutingHelper.GetStaticRouting(epcHelper->GetPgwNode()->GetObject<Ipv4>());
  enbStaticRouting->AddNetworkRouteTo(Ipv4Address("1.0.0.0"), Ipv4Mask("255.255.255.0"), 1);

  lteHelper->Attach(ueDevs.Get(0), enbDevs.Get(0));
  lteHelper->Attach(ueDevs.Get(1), enbDevs.Get(0));

  uint16_t dlPort = 5000;
  uint16_t ulPort = 5001;

  ApplicationContainer clientApps;
  ApplicationContainer serverApps;

  UdpClientHelper dlClient(ueIpIface.GetAddress(0), dlPort);
  dlClient.SetAttribute("MaxPackets", UintegerValue(1000000));
  dlClient.SetAttribute("Interval", TimeValue(MilliSeconds(1)));
  dlClient.SetAttribute("PacketSize", UintegerValue(1024));
  clientApps.Add(dlClient.Install(ueNodes.Get(1)));

  UdpServerHelper dlServer(dlPort);
  serverApps.Add(dlServer.Install(ueNodes.Get(0)));

  UdpClientHelper ulClient(ueIpIface.GetAddress(1), ulPort);
  ulClient.SetAttribute("MaxPackets", UintegerValue(1000000));
  ulClient.SetAttribute("Interval", TimeValue(MilliSeconds(1)));
  ulClient.SetAttribute("PacketSize", UintegerValue(1024));
  clientApps.Add(ulClient.Install(ueNodes.Get(0)));

  UdpServerHelper ulServer(ulPort);
  serverApps.Add(ulServer.Install(ueNodes.Get(1)));

  serverApps.Start(Seconds(1.0));
  clientApps.Start(Seconds(2.0));

  AnimationInterface anim("lte-netanim.xml");
  anim.SetConstantPosition(enbNodes.Get(0), 10.0, 20.0);
  anim.SetConstantPosition(ueNodes.Get(0), 1.0, 1.0);
  anim.SetConstantPosition(ueNodes.Get(1), 20.0, 30.0);
  anim.SetConstantPosition(epcHelper->GetPgwNode(), 50.0, 20.0);

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}