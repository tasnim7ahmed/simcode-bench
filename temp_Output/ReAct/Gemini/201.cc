#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include <iostream>
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("StarTopology");

int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponent::SetLogLevel (LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create (5);

  NodeContainer hubNode = NodeContainer (nodes.Get (0));
  NodeContainer spokeNodes;
  spokeNodes.Add (nodes.Get (1));
  spokeNodes.Add (nodes.Get (2));
  spokeNodes.Add (nodes.Get (3));
  spokeNodes.Add (nodes.Get (4));

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("1ms"));

  NetDeviceContainer hubDevices;
  NetDeviceContainer spokeDevices;

  for (int i = 1; i < 5; ++i)
    {
      NetDeviceContainer link = pointToPoint.Install (nodes.Get (0), nodes.Get (i));
      hubDevices.Add (link.Get (0));
      spokeDevices.Add (link.Get (1));
    }

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer hubInterfaces;
  Ipv4InterfaceContainer spokeInterfaces;

  for (uint32_t i = 0; i < 4; ++i)
    {
      Ipv4InterfaceContainer interface = address.Assign (NetDeviceContainer (hubDevices.Get (i), spokeDevices.Get (i)));
      hubInterfaces.Add (interface.Get (0));
      spokeInterfaces.Add (interface.Get (1));
      address.NewNetwork ();
    }

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  UdpEchoServerHelper echoServer (9);
  ApplicationContainer serverApps = echoServer.Install (hubNode);
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  UdpEchoClientHelper echoClient (hubInterfaces.GetAddress (0), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (1000));
  echoClient.SetAttribute ("Interval", TimeValue (MilliSeconds (1)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps;
  for (uint32_t i = 0; i < 4; ++i)
    {
      clientApps.Add (echoClient.Install (spokeNodes.Get (i)));
    }

  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  Simulator::Stop (Seconds (10.0));

  FlowMonitorHelper flowMonitor;
  Ptr<FlowMonitor> monitor = flowMonitor.InstallAll ();

  pointToPoint.EnablePcapAll ("star");

  Simulator::Run ();

  monitor->CheckForLostPackets ();

  std::ofstream os;
  os.open ("star.flowmon");
  monitor->SerializeToXmlFile ("star.flowmon", false, true);
  os.close ();

  Simulator::Destroy ();
  return 0;
}