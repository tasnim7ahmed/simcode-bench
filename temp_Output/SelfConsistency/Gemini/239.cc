#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/tcp-socket-factory.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpPointToPoint");

int main(int argc, char* argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);
  LogComponent::SetLogLevel(LOG_LEVEL_INFO);
  LogComponent::SetPrintPrefix(true);

  // Create nodes
  NodeContainer nodes;
  nodes.Create(3); // client, router, server

  NodeContainer clientRouter;
  clientRouter.Add(nodes.Get(0));
  clientRouter.Add(nodes.Get(1));

  NodeContainer routerServer;
  routerServer.Add(nodes.Get(1));
  routerServer.Add(nodes.Get(2));

  // Create channels
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer clientRouterDevices;
  clientRouterDevices = pointToPoint.Install(clientRouter);

  NetDeviceContainer routerServerDevices;
  routerServerDevices = pointToPoint.Install(routerServer);

  // Install Internet stack
  InternetStackHelper stack;
  stack.Install(nodes);

  // Assign addresses
  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer clientRouterInterfaces;
  clientRouterInterfaces = address.Assign(clientRouterDevices);

  address.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer routerServerInterfaces;
  routerServerInterfaces = address.Assign(routerServerDevices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // Create client application
  uint16_t port = 9; // well-known echo port number
  Address serverAddress(
      InetSocketAddress(routerServerInterfaces.GetAddress(1), port));

  TcpEchoClientHelper echoClientHelper(serverAddress);
  echoClientHelper.SetAttribute("MessageSize", UintegerValue(1024));
  echoClientHelper.SetAttribute("MaxPackets", UintegerValue(100));
  echoClientHelper.SetAttribute("Interval", TimeValue(Seconds(1.)));

  ApplicationContainer clientApps = echoClientHelper.Install(nodes.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  // Create server application
  TcpEchoServerHelper echoServerHelper(port);
  ApplicationContainer serverApps = echoServerHelper.Install(nodes.Get(2));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(11.0));

  Simulator::Stop(Seconds(12.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}