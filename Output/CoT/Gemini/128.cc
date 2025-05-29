#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/olsr-helper.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("LSAOSPFSimulation");

int main (int argc, char *argv[])
{
  bool enablePcap = true;
  bool enableNetAnim = true;

  CommandLine cmd;
  cmd.AddValue ("enablePcap", "Enable PCAP tracing", enablePcap);
  cmd.AddValue ("enableNetAnim", "Enable NetAnim visualization", enableNetAnim);
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponent::SetFilter ("OlsrRoutingProtocol", LOG_LEVEL_ALL);

  NodeContainer routers;
  routers.Create (4);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer devices01 = pointToPoint.Install (routers.Get (0), routers.Get (1));
  NetDeviceContainer devices02 = pointToPoint.Install (routers.Get (0), routers.Get (2));
  NetDeviceContainer devices13 = pointToPoint.Install (routers.Get (1), routers.Get (3));
  NetDeviceContainer devices23 = pointToPoint.Install (routers.Get (2), routers.Get (3));

  InternetStackHelper internet;
  OlsrHelper olsr;
  internet.SetRoutingHelper (olsr);
  internet.Install (routers);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i01 = ipv4.Assign (devices01);

  ipv4.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer i02 = ipv4.Assign (devices02);

  ipv4.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer i13 = ipv4.Assign (devices13);

  ipv4.SetBase ("10.1.4.0", "255.255.255.0");
  Ipv4InterfaceContainer i23 = ipv4.Assign (devices23);

  if (enablePcap)
  {
    pointToPoint.EnablePcapAll ("lsa-ospf");
  }

  UdpEchoServerHelper echoServer (9);
  ApplicationContainer serverApps = echoServer.Install (routers.Get (3));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  UdpEchoClientHelper echoClient (i23.GetAddress (1), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (100));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (0.01)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientApps = echoClient.Install (routers.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  Simulator::Stop (Seconds (10.0));

  if(enableNetAnim) {
      AnimationInterface anim ("lsa-ospf.xml");
      anim.SetConstantPosition(routers.Get(0), 10, 10);
      anim.SetConstantPosition(routers.Get(1), 40, 10);
      anim.SetConstantPosition(routers.Get(2), 10, 40);
      anim.SetConstantPosition(routers.Get(3), 40, 40);
  }

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  Simulator::Run ();

  monitor->CheckForLostPackets ();

  Ptr<Ipv4GlobalRouting> globalRouting = Ipv4GlobalRouting::GetInstance ();
  if (globalRouting)
  {
      globalRouting->PrintRoutingTableAllAt (Seconds (10.0), std::cout);
  }

  Ptr<OutputStreamWrapper> routingStream = Create<OutputStreamWrapper> ("olsr-routing.routes", std::ios::out);
  olsr.PrintRoutingTableAllAt (Seconds (10.0), routingStream);

  monitor->SerializeToXmlFile("lsa-ospf-flowmon.xml", true, true);

  Simulator::Destroy ();
  return 0;
}