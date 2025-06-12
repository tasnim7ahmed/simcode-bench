#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/nat-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/udp-client-server-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);

  NodeContainer privateNetworkNodes, publicNetworkNodes, natRouterNode;
  privateNetworkNodes.Create(2);
  publicNetworkNodes.Create(1);
  natRouterNode.Create(1);

  NodeContainer privateNetwork = NodeContainer(privateNetworkNodes, natRouterNode.Get(0));
  NodeContainer publicNetwork = NodeContainer(natRouterNode.Get(0), publicNetworkNodes.Get(0));

  PointToPointHelper privateNetworkP2P, publicNetworkP2P;
  privateNetworkP2P.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
  privateNetworkP2P.SetChannelAttribute("Delay", StringValue("2ms"));

  publicNetworkP2P.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
  publicNetworkP2P.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer privateDevices = privateNetworkP2P.Install(privateNetwork);
  NetDeviceContainer publicDevices = publicNetworkP2P.Install(publicNetwork);

  InternetStackHelper stack;
  stack.Install(privateNetworkNodes);
  stack.Install(publicNetworkNodes);
  stack.Install(natRouterNode);

  Ipv4AddressHelper privateNetworkAddress, publicNetworkAddress;
  privateNetworkAddress.SetBase("10.1.1.0", "255.255.255.0");
  publicNetworkAddress.SetBase("192.168.1.0", "255.255.255.0");

  Ipv4InterfaceContainer privateInterfaces = privateNetworkAddress.Assign(privateDevices);
  Ipv4InterfaceContainer publicInterfaces = publicNetworkAddress.Assign(publicDevices);

  Ipv4Address privateHost1Address = privateInterfaces.GetAddress(0);
  Ipv4Address privateHost2Address = privateInterfaces.GetAddress(1);
  Ipv4Address natRouterPrivateAddress = privateInterfaces.GetAddress(2);
  Ipv4Address natRouterPublicAddress = publicInterfaces.GetAddress(0);
  Ipv4Address publicServerAddress = publicInterfaces.GetAddress(1);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  NatHelper natHelper;
  Ipv4NatRouter *natRouter = natHelper.Install(natRouterNode.Get(0))->GetObject<Ipv4NatRouter>(0);
  natRouter->SetInsideAddress(natRouterPrivateAddress);

  Ptr<NetDevice> publicNetDevice = publicDevices.Get(0);
  natRouter->AddOutboundInterface(publicNetDevice);

  UdpClientServerHelper client(publicServerAddress, 9);
  client.SetAttribute("MaxPackets", UintegerValue(1));
  client.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  client.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps = client.Install(privateNetworkNodes.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  UdpClientServerHelper client2(publicServerAddress, 9);
  client2.SetAttribute("MaxPackets", UintegerValue(1));
  client2.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  client2.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps2 = client2.Install(privateNetworkNodes.Get(1));
  clientApps2.Start(Seconds(3.0));
  clientApps2.Stop(Seconds(10.0));

  UdpServerHelper server(9);
  ApplicationContainer serverApps = server.Install(publicNetworkNodes.Get(0));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  Simulator::Stop(Seconds(11.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}