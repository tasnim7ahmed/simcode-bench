#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/olsr-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/traffic-control-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("LsaSimulation");

int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponent::SetAttribute ("OlsrRoutingProtocol",
                               "EnableHelloMessagesLogging", BooleanValue (true));
  LogComponent::SetAttribute ("OlsrRoutingProtocol",
                               "EnableTcMessagesLogging", BooleanValue (true));

  NodeContainer routers;
  routers.Create (4);

  PointToPointHelper p2p;
  p2p.DataRate ("10Mbps");
  p2p.Delay ("2ms");
  p2p.QueueDisc ("ns3::FifoQueueDisc", "MaxSize", StringValue("1000p"));

  NetDeviceContainer r0r1 = p2p.Install (routers.Get (0), routers.Get (1));
  NetDeviceContainer r0r2 = p2p.Install (routers.Get (0), routers.Get (2));
  NetDeviceContainer r1r3 = p2p.Install (routers.Get (1), routers.Get (3));
  NetDeviceContainer r2r3 = p2p.Install (routers.Get (2), routers.Get (3));

  InternetStackHelper internet;
  internet.Install (routers);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i0i1 = ipv4.Assign (r0r1);

  ipv4.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer i0i2 = ipv4.Assign (r0r2);

  ipv4.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer i1i3 = ipv4.Assign (r1r3);

  ipv4.SetBase ("10.1.4.0", "255.255.255.0");
  Ipv4InterfaceContainer i2i3 = ipv4.Assign (r2r3);

  OlsrHelper olsr;
  OlsrHelper::RoutingProtocolType routingProtocol = OlsrHelper::RoutingProtocolType::OLSRv2;
  olsr.Set ("RoutingProtocol", EnumValue (routingProtocol));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  ApplicationContainer sinkApp;
  Address sinkAddress (InetSocketAddress (i2i3.GetAddress (1), 9));
  PacketSinkHelper packetSinkHelper ("ns3::UdpSocketFactory", sinkAddress);
  sinkApp = packetSinkHelper.Install (routers.Get (3));
  sinkApp.Start (Seconds (1.0));
  sinkApp.Stop (Seconds (10.0));

  Ptr<Socket> ns3UdpSocket = Socket::CreateSocket (routers.Get (0), UdpSocketFactory::GetTypeId ());
  Ptr<ConstantRateWifiTrafficGenerator> trafficGenerator = CreateObject<ConstantRateWifiTrafficGenerator> ();
  trafficGenerator->SetAttribute ("SendTime", TimeValue (Seconds (9.0)));
  trafficGenerator->SetAttribute ("OnTime",  StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  trafficGenerator->SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
  trafficGenerator->SetAttribute ("PacketSize", UintegerValue (1024));
  trafficGenerator->SetAttribute ("DataRate", DataRateValue (DataRate ("1Mbps")));
  trafficGenerator->SetAttribute ("RemoteAddress", AddressValue (InetSocketAddress (i2i3.GetAddress (1), 9)));
  trafficGenerator->SetAttribute ("Socket", PointerValue (ns3UdpSocket));
  routers.Get(0)->AddApplication (trafficGenerator);
  trafficGenerator->StartApplication (Seconds (2.0));
  trafficGenerator->StopApplication (Seconds (10.0));


  p2p.EnablePcapAll ("lsa-simulation", false);

  AnimationInterface anim ("lsa-simulation.xml");
  anim.SetConstantPosition (routers.Get (0), 10, 10);
  anim.SetConstantPosition (routers.Get (1), 30, 10);
  anim.SetConstantPosition (routers.Get (2), 10, 30);
  anim.SetConstantPosition (routers.Get (3), 30, 30);

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}