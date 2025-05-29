#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/nat-module.h"
#include "ns3/udp-client-server-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);

  NodeContainer privateNetworkNodes;
  privateNetworkNodes.Create(2);

  NodeContainer natRouter;
  natRouter.Create(1);

  NodeContainer publicServer;
  publicServer.Create(1);

  PointToPointHelper privateNetworkLink;
  privateNetworkLink.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  privateNetworkLink.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer privateDevices = privateNetworkLink.Install(privateNetworkNodes.Get(0), natRouter.Get(0));
  NetDeviceContainer privateDevices2 = privateNetworkLink.Install(privateNetworkNodes.Get(1), natRouter.Get(0));

  PointToPointHelper publicNetworkLink;
  publicNetworkLink.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
  publicNetworkLink.SetChannelAttribute("Delay", StringValue("1ms"));

  NetDeviceContainer publicDevices = publicNetworkLink.Install(natRouter.Get(0), publicServer.Get(0));

  InternetStackHelper internet;
  internet.Install(privateNetworkNodes);
  internet.Install(natRouter);
  internet.Install(publicServer);

  Ipv4AddressHelper privateAddress;
  privateAddress.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer privateInterfaces = privateAddress.Assign(privateDevices);
  Ipv4InterfaceContainer privateInterfaces2 = privateAddress.Assign(privateDevices2);

  Ipv4AddressHelper publicAddress;
  publicAddress.SetBase("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer publicInterfaces = publicAddress.Assign(publicDevices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // NAT configuration
  Ptr<Node> natRouterNode = natRouter.Get(0);
  Ptr<Nat> nat = CreateObject<Nat>();
  natRouterNode->AddApplication(nat);
  nat->SetDevice(publicInterfaces.Get(0));
  nat->SetInternalAddress(Ipv4Address("10.1.1.0"), Ipv4Mask("255.255.255.0"));
  nat->Start(Seconds(0.0));
  nat->Stop(Seconds(10.0));

  UdpEchoServerHelper echoServer(9);
  ApplicationContainer serverApps = echoServer.Install(publicServer.Get(0));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  UdpEchoClientHelper echoClient(publicInterfaces.GetAddress(1), 9);
  echoClient.SetAttribute("MaxPackets", UintegerValue(5));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  echoClient.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps = echoClient.Install(privateNetworkNodes.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  UdpEchoClientHelper echoClient2(publicInterfaces.GetAddress(1), 9);
  echoClient2.SetAttribute("MaxPackets", UintegerValue(5));
  echoClient2.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  echoClient2.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps2 = echoClient2.Install(privateNetworkNodes.Get(1));
  clientApps2.Start(Seconds(2.0));
  clientApps2.Stop(Seconds(10.0));


  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}