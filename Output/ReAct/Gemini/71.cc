#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("ThreeRouterNetwork");

int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponent::SetCategory ("ThreeRouterNetwork");
  LogComponent::SetLevel (LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create (3);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer devicesAB = pointToPoint.Install (nodes.Get (0), nodes.Get (1));
  NetDeviceContainer devicesBC = pointToPoint.Install (nodes.Get (1), nodes.Get (2));

  CsmaHelper csmaA;
  csmaA.SetChannelAttribute ("DataRate", StringValue ("100Mbps"));
  csmaA.SetChannelAttribute ("Delay", StringValue ("10us"));
  NetDeviceContainer csmaDevicesA = csmaA.Install (nodes.Get (0));

  CsmaHelper csmaC;
  csmaC.SetChannelAttribute ("DataRate", StringValue ("100Mbps"));
  csmaC.SetChannelAttribute ("Delay", StringValue ("10us"));
  NetDeviceContainer csmaDevicesC = csmaC.Install (nodes.Get (2));


  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.252");
  Ipv4InterfaceContainer interfacesAB = address.Assign (devicesAB);

  address.SetBase ("10.1.1.4", "255.255.255.252");
  Ipv4InterfaceContainer interfacesBC = address.Assign (devicesBC);

  address.SetBase ("172.16.1.0", "255.255.255.0");
  Ipv4InterfaceContainer csmaInterfacesA = address.Assign (csmaDevicesA);

  address.SetBase ("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer csmaInterfacesC = address.Assign (csmaDevicesC);

  Ipv4Address a_address ("172.16.1.1");
  Ipv4Address c_address ("192.168.1.1");

  csmaInterfacesA.GetAddress (0);
  csmaInterfacesC.GetAddress (0);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  uint16_t port = 9;
  UdpClientHelper client (c_address, port);
  client.SetAttribute ("MaxPackets", UintegerValue (1000));
  client.SetAttribute ("Interval", TimeValue (Seconds (0.01)));
  client.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientApps = client.Install (nodes.Get (0));
  clientApps.Start (Seconds (1.0));
  clientApps.Stop (Seconds (10.0));

  UdpServerHelper echoServer (port);
  ApplicationContainer serverApps = echoServer.Install (nodes.Get (2));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  Simulator::Stop (Seconds (11.0));

  pointToPoint.EnablePcapAll ("three-router");
  csmaA.EnablePcap ("three-router-csma-A", csmaDevicesA.Get (0));
  csmaC.EnablePcap ("three-router-csma-C", csmaDevicesC.Get (0));


  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}