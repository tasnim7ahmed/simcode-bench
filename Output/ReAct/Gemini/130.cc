#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/nat-module.h"
#include "ns3/udp-client-server-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);
  LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
  LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
  LogComponentEnable("Nat", LOG_LEVEL_ALL);

  NodeContainer privateNodes, publicNodes;
  privateNodes.Create(2);
  publicNodes.Create(1);

  NodeContainer natRouter;
  natRouter.Create(1);

  PointToPointHelper privateLink, publicLink, natLink;
  privateLink.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
  privateLink.SetChannelAttribute("Delay", StringValue("2ms"));

  publicLink.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
  publicLink.SetChannelAttribute("Delay", StringValue("2ms"));

  natLink.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
  natLink.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer privateDevices1 = privateLink.Install(privateNodes.Get(0), natRouter.Get(0));
  NetDeviceContainer privateDevices2 = privateLink.Install(privateNodes.Get(1), natRouter.Get(0));
  NetDeviceContainer publicDevices = publicLink.Install(natRouter.Get(0), publicNodes.Get(0));

  InternetStackHelper stack;
  stack.Install(privateNodes);
  stack.Install(publicNodes);
  stack.Install(natRouter);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer privateInterfaces1 = address.Assign(privateDevices1);

  address.NewNetwork();
  address.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer privateInterfaces2 = address.Assign(privateDevices2);

  address.NewNetwork();
  address.SetBase("20.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer publicInterfaces = address.Assign(publicDevices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // NAT configuration
  Ptr<Node> natBox = natRouter.Get(0);
  Ptr<Nat> nat = CreateObject<Nat>();
  natBox->AddApplication(nat);
  nat->SetStartTime(Seconds(0.0));
  nat->SetExternalAddress(publicInterfaces.GetAddress(0));

  nat->AddInternalNetwork(Ipv4Address("10.1.1.0"), Ipv4Mask("255.255.255.0"));
  nat->AddInternalNetwork(Ipv4Address("10.1.2.0"), Ipv4Mask("255.255.255.0"));

  UdpClientServerHelper clientServer(50000);
  clientServer.SetAttribute("MaxPackets", UintegerValue(10));
  clientServer.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  clientServer.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps1 = clientServer.Install(privateNodes.Get(0), publicNodes.Get(0));
  clientApps1.Start(Seconds(2.0));
  clientApps1.Stop(Seconds(10.0));

  ApplicationContainer clientApps2 = clientServer.Install(privateNodes.Get(1), publicNodes.Get(0));
  clientApps2.Start(Seconds(3.0));
  clientApps2.Stop(Seconds(10.0));

  UdpServerHelper server(50000);
  ApplicationContainer serverApps = server.Install(publicNodes.Get(0));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(11.0));

  AnimationInterface anim("nat-animation.xml");
  anim.SetConstantPosition(privateNodes.Get(0), 1, 1);
  anim.SetConstantPosition(privateNodes.Get(1), 1, 3);
  anim.SetConstantPosition(natRouter.Get(0), 3, 2);
  anim.SetConstantPosition(publicNodes.Get(0), 5, 2);

  Simulator::Stop(Seconds(11.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}