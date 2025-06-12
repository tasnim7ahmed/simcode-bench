#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("UdpEchoCsmaTraced");

static std::ofstream traceFile;

void
QueueTrace (Ptr<const QueueDisc> queue)
{
  traceFile << Simulator::Now ().GetSeconds ()
            << " QUEUE packets: " << queue->GetNPackets () << std::endl;
}

void
RxTrace (Ptr<const Packet> packet, const Address &address)
{
  traceFile << Simulator::Now ().GetSeconds ()
            << " PACKET received, size: " << packet->GetSize () << std::endl;
}

int
main (int argc, char *argv[])
{
  // Tracing setup
  traceFile.open ("udp-echo.tr");
  if (!traceFile.is_open ())
    {
      NS_FATAL_ERROR ("Cannot open trace file.");
    }

  // Simulation in real-time
  GlobalValue::Bind ("SimulatorImplementationType", StringValue ("ns3::RealtimeSimulatorImpl"));

  uint32_t nNodes = 4;

  // Nodes
  NodeContainer nodes;
  nodes.Create (nNodes);

  // CSMA Channel/Device
  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", DataRateValue (DataRate (100000000))); // 100Mbps
  csma.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (6560)));
  csma.SetQueue ("ns3::DropTailQueue", "MaxPackets", UintegerValue (100));

  // Install CSMA
  NetDeviceContainer devices = csma.Install (nodes);

  // Tracing: Queue
  for (uint32_t i = 0; i < devices.GetN (); ++i)
    {
      Ptr<NetDevice> dev = devices.Get (i);
      Ptr<CsmaNetDevice> csmaDev = DynamicCast<CsmaNetDevice> (dev);
      if (csmaDev)
        {
          Ptr<Queue<Packet> > queue = csmaDev->GetQueue ();
          if (!queue)
            continue;

          // Not using QueueDisc since CSMA does not use QueueDisc, so connect directly to drop tail queue trace
          queue->TraceConnectWithoutContext ("PacketsInQueue", MakeCallback (
              [](uint32_t oldValue, uint32_t newValue) {
                traceFile << Simulator::Now ().GetSeconds ()
                          << " QUEUE PacketsInQueue: " << newValue << std::endl;
              }
          ));
        }
    }

  // Internet stack
  InternetStackHelper stack;
  stack.Install (nodes);

  // Assign IP Addresses
  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  // UDP Echo Server on node 1
  uint16_t echoPort = 9;
  UdpEchoServerHelper echoServer (echoPort);
  ApplicationContainer serverApps = echoServer.Install (nodes.Get (1));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  // UDP Echo Client on node 0
  UdpEchoClientHelper echoClient (interfaces.GetAddress (1), echoPort);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (3));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientApps = echoClient.Install (nodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  // Tracing: Packet receptions at UDP server
  Ptr<Node> serverNode = nodes.Get (1);
  Ptr<Ipv4L3Protocol> ipv4 = serverNode->GetObject<Ipv4L3Protocol> ();
  ipv4->TraceConnectWithoutContext ("Rx", MakeCallback (&RxTrace));

  // PCAP Tracing
  csma.EnablePcapAll ("udp-echo", false);

  // Set logging components (optional)
  //LogComponentEnable ("UdpEchoCsmaTraced", LOG_LEVEL_INFO);

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  traceFile.close ();
  Simulator::Destroy ();
  return 0;
}