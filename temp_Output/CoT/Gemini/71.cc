#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(3);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer devicesAB = pointToPoint.Install(nodes.Get(0), nodes.Get(1));
  NetDeviceContainer devicesBC = pointToPoint.Install(nodes.Get(1), nodes.Get(2));

  CsmaHelper csma;
  NetDeviceContainer devicesA = csma.Install(nodes.Get(0));
  NetDeviceContainer devicesC = csma.Install(nodes.Get(2));

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.252");
  Ipv4InterfaceContainer interfacesAB = address.Assign(devicesAB);

  address.SetBase("10.1.1.4", "255.255.255.252");
  Ipv4InterfaceContainer interfacesBC = address.Assign(devicesBC);

  address.SetBase("172.16.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfacesA = address.Assign(devicesA);

  address.SetBase("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfacesC = address.Assign(devicesC);

  Ipv4Address aAddress("10.1.1.1");
  Ipv4Address bAddress1("10.1.1.2");
  Ipv4Address bAddress2("10.1.1.5");
  Ipv4Address cAddress("10.1.1.6");
  Ipv4Address aCsmaAddress("172.16.1.1");
  Ipv4Address cCsmaAddress("192.168.1.1");

  interfacesAB.GetAddress(0);
  interfacesBC.GetAddress(0);
  interfacesA.GetAddress(0);
  interfacesC.GetAddress(0);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  uint16_t port = 9;
  UdpEchoServerHelper echoServer(port);

  ApplicationContainer serverApps = echoServer.Install(nodes.Get(2));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  UdpEchoClientHelper echoClient(cCsmaAddress, port);
  echoClient.SetAttribute("MaxPackets", UintegerValue(100));
  echoClient.SetAttribute("Interval", TimeValue(MilliSeconds(10)));
  echoClient.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  Simulator::Stop(Seconds(11.0));

  pointToPoint.EnablePcapAll("router");

  Simulator::Run();
  Simulator::Destroy();
  return 0;
}