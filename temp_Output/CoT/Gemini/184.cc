#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/lte-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/uan-module.h"
#include "ns3/netanim-module.h"
#include "ns3/mobility-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {

  CommandLine cmd;
  cmd.Parse(argc, argv);

  Config::SetDefault ("ns3::LteEnbRrc::DefaultTransmissionMode", IntegerValue (1));

  NodeContainer enbNodes;
  enbNodes.Create(1);
  NodeContainer ueNodes;
  ueNodes.Create(2);
  NodeContainer remoteHostContainer;
  remoteHostContainer.Create(1);

  Ptr<Node> remoteHost = remoteHostContainer.Get(0);
  InternetStackHelper internet;
  internet.Install(remoteHostContainer);

  LteHelper lteHelper;
  lteHelper.SetEnbDeviceAttribute ("DlEarfcn", UintegerValue (100));
  lteHelper.SetEnbDeviceAttribute ("UlEarfcn", UintegerValue (18100));

  PointToPointHelper p2ph;
  p2ph.SetDeviceAttribute("DataRate", StringValue("100Gb/s"));
  p2ph.SetDeviceAttribute("Mtu", UintegerValue(1500));
  p2ph.SetChannelAttribute("Delay", TimeValue(Seconds(0.010)));

  NetDeviceContainer enbNetDev = lteHelper.InstallEnbDevice(enbNodes);
  NetDeviceContainer ueNetDev = lteHelper.InstallUeDevice(ueNodes);

  InternetStackHelper internetHelper;
  internetHelper.Install(ueNodes);
  internetHelper.Install(enbNodes);

  NetDeviceContainer p2pDevices = p2ph.Install(remoteHost, enbNodes.Get(0));

  Ipv4AddressHelper ipv4h;
  ipv4h.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer internetInterfaces = ipv4h.Assign(p2pDevices);
  Ipv4Address remoteHostAddr = internetInterfaces.GetAddress(0);

  ipv4h.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer ueInterfaces = ipv4h.Assign(ueNetDev);

  Ipv4StaticRoutingHelper ipv4RoutingHelper;
  Ptr<Ipv4StaticRouting> remoteHostRouting = ipv4RoutingHelper.GetStaticRouting(remoteHost->GetObject<Ipv4>());
  remoteHostRouting->AddNetworkRouteTo(Ipv4Address("10.1.2.0"), Ipv4Mask("255.255.255.0"), internetInterfaces.GetAddress(1), 1);

  lteHelper.Attach(ueNetDev, enbNetDev.Get(0));

  UdpClientHelper clientHelper(remoteHostAddr, 9);
  clientHelper.SetAttribute("MaxPackets", UintegerValue(1000));
  clientHelper.SetAttribute("Interval", TimeValue(MilliSeconds(10)));
  clientHelper.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps = clientHelper.Install(ueNodes);
  clientApps.Start(Seconds(1.0));
  clientApps.Stop(Seconds(10.0));

  UdpServerHelper serverHelper(9);
  ApplicationContainer serverApps = serverHelper.Install(remoteHost);
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  MobilityHelper mobility;
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(enbNodes);

  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(remoteHostContainer);

  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(ueNodes);

  AnimationInterface anim("lte-netanim.xml");
  anim.SetConstantPosition(enbNodes.Get(0), 10.0, 10.0);
  anim.SetConstantPosition(ueNodes.Get(0), 20.0, 20.0);
  anim.SetConstantPosition(ueNodes.Get(1), 30.0, 30.0);
  anim.SetConstantPosition(remoteHostContainer.Get(0), 40.0, 10.0);

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}