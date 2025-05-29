#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-layout-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("UdpEchoCsmaExample");

static void
QueueEnqueueTrace (Ptr<const Packet> packet)
{
  static std::ofstream out ("udp-echo.tr", std::ios_base::app);
  out << Simulator::Now ().GetSeconds () << " ENQUEUE packet uid: " << packet->GetUid () << std::endl;
}

static void
QueueDequeueTrace (Ptr<const Packet> packet)
{
  static std::ofstream out ("udp-echo.tr", std::ios_base::app);
  out << Simulator::Now ().GetSeconds () << " DEQUEUE packet uid: " << packet->GetUid () << std::endl;
}

static void
QueueDropTrace (Ptr<const Packet> packet)
{
  static std::ofstream out ("udp-echo.tr", std::ios_base::app);
  out << Simulator::Now ().GetSeconds () << " DROP packet uid: " << packet->GetUid () << std::endl;
}

static void
RxTrace (Ptr<const Packet> packet, const Address &address)
{
  static std::ofstream out ("udp-echo.tr", std::ios_base::app);
  out << Simulator::Now ().GetSeconds () << " RX packet uid: " << packet->GetUid () << std::endl;
}

int
main (int argc, char *argv[])
{
  LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);

  // Use real-time simulation
  GlobalValue::Bind ("SimulatorImplementationType", StringValue ("ns3::RealtimeSimulatorImpl"));

  // Create nodes
  NodeContainer nodes;
  nodes.Create (4);

  // Configure CSMA LAN with DropTail queues
  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", StringValue ("100Mbps"));
  csma.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (6560)));
  csma.SetQueue ("ns3::DropTailQueue<Packet>");

  // Install devices and channels
  NetDeviceContainer csmaDevices = csma.Install (nodes);

  // Install Internet stack
  InternetStackHelper stack;
  stack.Install (nodes);

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (csmaDevices);

  // Set up tracing
  Ptr<NetDevice> dev0 = csmaDevices.Get (0);
  Ptr<NetDevice> dev1 = csmaDevices.Get (1);
  Ptr<CsmaNetDevice> csmaDev0 = DynamicCast<CsmaNetDevice> (dev0);
  Ptr<CsmaNetDevice> csmaDev1 = DynamicCast<CsmaNetDevice> (dev1);

  Ptr<Queue<Packet>> queue0 = csmaDev0->GetQueue ();
  Ptr<Queue<Packet>> queue1 = csmaDev1->GetQueue ();

  if (queue0)
    {
      queue0->TraceConnectWithoutContext ("Enqueue", MakeCallback (&QueueEnqueueTrace));
      queue0->TraceConnectWithoutContext ("Dequeue", MakeCallback (&QueueDequeueTrace));
      queue0->TraceConnectWithoutContext ("Drop", MakeCallback (&QueueDropTrace));
    }
  if (queue1)
    {
      queue1->TraceConnectWithoutContext ("Enqueue", MakeCallback (&QueueEnqueueTrace));
      queue1->TraceConnectWithoutContext ("Dequeue", MakeCallback (&QueueDequeueTrace));
      queue1->TraceConnectWithoutContext ("Drop", MakeCallback (&QueueDropTrace));
    }

  csmaDev1->TraceConnectWithoutContext ("MacRx", MakeCallback (&RxTrace));
  csmaDev0->TraceConnectWithoutContext ("MacRx", MakeCallback (&RxTrace));

  // Enable pcap tracing
  csma.EnablePcapAll ("udp-echo", false);

  // Install UDP echo server on node 1
  uint16_t echoPort = 9;
  UdpEchoServerHelper echoServer (echoPort);
  ApplicationContainer serverApps = echoServer.Install (nodes.Get (1));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  // Install UDP echo client on node 0
  UdpEchoClientHelper echoClient (interfaces.GetAddress (1), echoPort);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (5));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps = echoClient.Install (nodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  Simulator::Stop (Seconds (11.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}