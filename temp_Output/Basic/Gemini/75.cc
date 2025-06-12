#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("MixedNetwork");

int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponent::SetFilter ("UdpClient", LOG_LEVEL_INFO);

  NodeContainer p2pNodes02;
  p2pNodes02.Create (2);

  NodeContainer p2pNodes56;
  p2pNodes56.Create (2);

  NodeContainer csmaNodes;
  csmaNodes.Add (p2pNodes02.Get (1));
  csmaNodes.Create (4);

  InternetStackHelper stack;
  stack.Install (p2pNodes02.Get (0));
  stack.Install (csmaNodes);
  stack.Install (p2pNodes56);

  PointToPointHelper pointToPoint02;
  pointToPoint02.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint02.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer p2pDevices02;
  p2pDevices02 = pointToPoint02.Install (p2pNodes02);

  PointToPointHelper pointToPoint56;
  pointToPoint56.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint56.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer p2pDevices56;
  p2pDevices56 = pointToPoint56.Install (p2pNodes56);

  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", StringValue ("5Mbps"));
  csma.SetChannelAttribute ("Delay", StringValue ("3ms"));

  NetDeviceContainer csmaDevices;
  csmaDevices = csma.Install (csmaNodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer p2pInterfaces02;
  p2pInterfaces02 = address.Assign (p2pDevices02);

  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer csmaInterfaces;
  csmaInterfaces = address.Assign (csmaDevices);

  address.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer p2pInterfaces56;
  p2pInterfaces56 = address.Assign (p2pDevices56);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  uint16_t port = 9;

  UdpClientHelper client (p2pInterfaces56.GetAddress (1), port);
  client.SetAttribute ("MaxPackets", UintegerValue (1000000));
  client.SetAttribute ("Interval", TimeValue (Time ("1ms")));
  client.SetAttribute ("PacketSize", UintegerValue (210));

  ApplicationContainer clientApps = client.Install (p2pNodes02.Get (0));
  clientApps.Start (Seconds (1.0));
  clientApps.Stop (Seconds (10.0));

  UdpServerHelper server (port);
  ApplicationContainer serverApps = server.Install (p2pNodes56.Get (1));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  Simulator::Stop (Seconds (10.0));

  AsciiTraceHelper ascii;
  pointToPoint02.EnableAsciiAll (ascii.CreateFileStream ("mixed-network.tr"));
  csma.EnableAsciiAll (ascii.CreateFileStream ("mixed-network.tr"));
  pointToPoint56.EnableAsciiAll (ascii.CreateFileStream ("mixed-network.tr"));

  csma.EnablePcapAll ("mixed-network", false);
  pointToPoint02.EnablePcapAll ("mixed-network", false);
  pointToPoint56.EnablePcapAll ("mixed-network", false);

  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}