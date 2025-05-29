#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("DistanceVectorRouting");

int main (int argc, char *argv[])
{
  LogComponent::SetLogLevel(LOG_LEVEL_INFO);
  CommandLine cmd;
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  Config::SetDefault ("ns3::RateAdaptationQueue::MaxPackets", UintegerValue (2000));

  NodeContainer routers;
  routers.Create (3);

  NodeContainer subnet1Nodes;
  subnet1Nodes.Create (1);

  NodeContainer subnet2Nodes;
  subnet2Nodes.Create (1);

  NodeContainer subnet3Nodes;
  subnet3Nodes.Create (1);

  NodeContainer subnet4Nodes;
  subnet4Nodes.Create (1);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer r1r2Devices = p2p.Install (routers.Get (0), routers.Get (1));
  NetDeviceContainer r2r3Devices = p2p.Install (routers.Get (1), routers.Get (2));
  NetDeviceContainer r1s1Devices = p2p.Install (routers.Get (0), subnet1Nodes.Get (0));
  NetDeviceContainer r2s2Devices = p2p.Install (routers.Get (1), subnet2Nodes.Get (0));
  NetDeviceContainer r2s3Devices = p2p.Install (routers.Get (2), subnet3Nodes.Get (0));
  NetDeviceContainer r3s4Devices = p2p.Install (routers.Get (2), subnet4Nodes.Get (0));

  InternetStackHelper internet;
  internet.Install (routers);
  internet.Install (subnet1Nodes);
  internet.Install (subnet2Nodes);
  internet.Install (subnet3Nodes);
  internet.Install (subnet4Nodes);

  Ipv4AddressHelper ipv4;

  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer r1r2Interfaces = ipv4.Assign (r1r2Devices);

  ipv4.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer r2r3Interfaces = ipv4.Assign (r2r3Devices);

  ipv4.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer r1s1Interfaces = ipv4.Assign (r1s1Devices);

  ipv4.SetBase ("10.1.4.0", "255.255.255.0");
  Ipv4InterfaceContainer r2s2Interfaces = ipv4.Assign (r2s2Devices);

   ipv4.SetBase ("10.1.5.0", "255.255.255.0");
  Ipv4InterfaceContainer r3s3Interfaces = ipv4.Assign (r2s3Devices);

  ipv4.SetBase ("10.1.6.0", "255.255.255.0");
  Ipv4InterfaceContainer r3s4Interfaces = ipv4.Assign (r3s4Devices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  UdpEchoServerHelper echoServer (9);
  ApplicationContainer serverApps = echoServer.Install (subnet4Nodes.Get (0));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  UdpEchoClientHelper echoClient (r3s4Interfaces.GetAddress (1), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (10));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientApps = echoClient.Install (subnet1Nodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  p2p.EnablePcapAll ("dvr");

  Simulator::Stop (Seconds (11.0));

  AnimationInterface anim ("dvr.xml");
  anim.SetConstantPosition(subnet1Nodes.Get(0), 0.0, 10.0);
  anim.SetConstantPosition(subnet2Nodes.Get(0), 20.0, 10.0);
  anim.SetConstantPosition(subnet3Nodes.Get(0), 40.0, 10.0);
  anim.SetConstantPosition(subnet4Nodes.Get(0), 60.0, 10.0);

  anim.SetConstantPosition(routers.Get(0), 10.0, 20.0);
  anim.SetConstantPosition(routers.Get(1), 30.0, 20.0);
  anim.SetConstantPosition(routers.Get(2), 50.0, 20.0);

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  Simulator::Run ();

  monitor->CheckForLostPackets ();

  Ptr<Ipv4GlobalRouting> globalRouting = Ipv4GlobalRouting::GetObject<Ipv4GlobalRouting>(routers.Get(0)->GetObject<Ipv4>()->GetRoutingProtocol());
  if (globalRouting)
  {
    globalRouting->PrintRoutingTable(std::cout);
  }

   monitor->SerializeToXmlFile("dvr.flowmon", true, true);

  Simulator::Destroy ();
  return 0;
}