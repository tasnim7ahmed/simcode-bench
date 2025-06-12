#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("ThreeRouterNetwork");

int
main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponent::SetFilter ("ThreeRouterNetwork", LOG_LEVEL_INFO);

  // Create nodes
  NS_LOG_INFO ("Create nodes.");
  NodeContainer routers;
  routers.Create (3);
  NodeContainer sender;
  sender.Create (1);
  NodeContainer receiver;
  receiver.Create (1);

  // Create point-to-point helper
  NS_LOG_INFO ("Create channels.");
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  // Connect A to B
  NetDeviceContainer devicesAB = p2p.Install (sender.Get (0), routers.Get (0));

  // Connect B to A
  NetDeviceContainer devicesBA = p2p.Install (routers.Get (0), routers.Get (1));

  // Connect B to C
  NetDeviceContainer devicesBC = p2p.Install (routers.Get (1), routers.Get (2));

  // Connect C to B
  NetDeviceContainer devicesCB = p2p.Install (routers.Get (2), receiver.Get (0));

  // Install internet stack
  NS_LOG_INFO ("Install Internet Stack.");
  InternetStackHelper internet;
  internet.Install (routers);
  internet.Install (sender);
  internet.Install (receiver);

  // Assign addresses
  NS_LOG_INFO ("Assign IP Addresses.");
  Ipv4AddressHelper ipv4;

  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer iAB = ipv4.Assign (devicesAB);

  ipv4.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer iBA = ipv4.Assign (devicesBA);

  ipv4.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer iBC = ipv4.Assign (devicesBC);

  ipv4.SetBase ("10.1.4.0", "255.255.255.0");
  Ipv4InterfaceContainer iCB = ipv4.Assign (devicesCB);

  // Set single addresses to end nodes
  Ptr<Node> nSender = sender.Get (0);
  Ptr<Node> nReceiver = receiver.Get (0);

  Ptr<Ipv4> ipv4Sender = nSender->GetObject<Ipv4> ();
  Ipv4Address addrSender = Ipv4Address ("1.1.1.1");
  ipv4Sender->SetManualConfiguration ();
  ipv4Sender->AddAddress (0, addrSender, Ipv4Mask ("255.255.255.255"));
  ipv4Sender->SetForwarding (0, true);
  ipv4Sender->SetDefaultRoute (0, Ipv4Address ("10.1.1.2"));

  Ptr<Ipv4> ipv4Receiver = nReceiver->GetObject<Ipv4> ();
  Ipv4Address addrReceiver = Ipv4Address ("4.4.4.4");
  ipv4Receiver->SetManualConfiguration ();
  ipv4Receiver->AddAddress (0, addrReceiver, Ipv4Mask ("255.255.255.255"));
  ipv4Receiver->SetForwarding (0, true);
  ipv4Receiver->SetDefaultRoute (0, Ipv4Address ("10.1.4.1"));

  // Populate routing tables
  NS_LOG_INFO ("Populate routing tables.");
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // Create OnOff application
  NS_LOG_INFO ("Create Applications.");
  uint16_t port = 9;  // Discard port (RFC 1350)
  OnOffHelper onoff ("ns3::UdpSocketFactory",
                     Address (InetSocketAddress (addrReceiver, port)));
  onoff.SetConstantRate (DataRate ("1Mbps"));

  ApplicationContainer apps = onoff.Install (sender.Get (0));
  apps.Start (Seconds (1.0));
  apps.Stop (Seconds (10.0));

  // Create PacketSink application
  PacketSinkHelper sink ("ns3::UdpSocketFactory",
                         InetSocketAddress (Ipv4Address::GetAny (), port));
  apps = sink.Install (receiver.Get (0));
  apps.Start (Seconds (0.0));
  apps.Stop (Seconds (10.0));

  // Tracing
  NS_LOG_INFO ("Configure tracing.");
  p2p.EnablePcapAll ("three-router");

  // Animation
  // AnimationInterface anim ("three-router.xml");

  // Run simulation
  NS_LOG_INFO ("Run Simulation.");
  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();
  NS_LOG_INFO ("Done.");

  return 0;
}