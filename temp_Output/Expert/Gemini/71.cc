#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/netanim-module.h"
#include <iostream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("ThreeRouterSimulation");

int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  NodeContainer nodes;
  nodes.Create (3);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer devicesAB = pointToPoint.Install (nodes.Get (0), nodes.Get (1));
  NetDeviceContainer devicesBC = pointToPoint.Install (nodes.Get (1), nodes.Get (2));

  CsmaHelper csmaA;
  csmaA.SetChannelAttribute ("DataRate", StringValue ("5Mbps"));
  csmaA.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (6560)));
  NetDeviceContainer csmaDevicesA = csmaA.Install (nodes.Get (0));

  CsmaHelper csmaC;
  csmaC.SetChannelAttribute ("DataRate", StringValue ("5Mbps"));
  csmaC.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (6560)));
  NetDeviceContainer csmaDevicesC = csmaC.Install (nodes.Get (2));

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.252");
  Ipv4InterfaceContainer interfacesAB = address.Assign (devicesAB);

  address.SetBase ("10.1.2.0", "255.255.255.252");
  Ipv4InterfaceContainer interfacesBC = address.Assign (devicesBC);

  address.SetBase ("172.16.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfacesCsmaA = address.Assign (csmaDevicesA);

  address.SetBase ("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfacesCsmaC = address.Assign (csmaDevicesC);

  Ipv4Address aAddress ("1.1.1.1");
  Ipv4Address cAddress ("2.2.2.2");
  interfacesAB.GetAddress (0,0);
  interfacesBC.GetAddress (1,0);
  interfacesCsmaA.GetAddress (0,0);
  interfacesCsmaC.GetAddress (0,0);
  
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  uint16_t port = 9;

  UdpClientHelper client (interfacesCsmaC.GetAddress (0), port);
  client.SetAttribute ("MaxPackets", UintegerValue (1000));
  client.SetAttribute ("Interval", TimeValue (MilliSeconds (10)));
  client.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientApps = client.Install (nodes.Get (0));
  clientApps.Start (Seconds (1.0));
  clientApps.Stop (Seconds (10.0));

  UdpServerHelper echoServer (port);
  ApplicationContainer serverApps = echoServer.Install (nodes.Get (2));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  Simulator::Stop (Seconds (10.0));

  pointToPoint.EnablePcapAll("three-router");
  csmaA.EnablePcapAll("three-router");
  csmaC.EnablePcapAll("three-router");

  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}