#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-layout-module.h"
#include <fstream>

using namespace ns3;

std::ofstream traceFile;

void
QueueEnqueueTrace(Ptr<const QueueItem> item)
{
  traceFile << Simulator::Now().GetSeconds() << " QueueEnqueue: PacketUid=" << item->GetPacket()->GetUid() << std::endl;
}

void
QueueDequeueTrace(Ptr<const QueueItem> item)
{
  traceFile << Simulator::Now().GetSeconds() << " QueueDequeue: PacketUid=" << item->GetPacket()->GetUid() << std::endl;
}

void
PacketRxTrace(Ptr<const Packet> packet, const Address &address)
{
  traceFile << Simulator::Now().GetSeconds() << " PacketRx: PacketUid=" << packet->GetUid() << std::endl;
}

int
main(int argc, char *argv[])
{
  Time::SetResolution(Time::NS);
  LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

  // Real-time simulation mode
  GlobalValue::Bind("SimulatorImplementationType", StringValue("ns3::RealtimeSimulatorImpl"));

  // Create nodes
  NodeContainer nodes;
  nodes.Create(4);

  // Create CSMA channel
  CsmaHelper csma;
  csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
  csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));
  csma.SetQueue("ns3::DropTailQueue", "MaxPackets", UintegerValue(100));

  NetDeviceContainer devices = csma.Install(nodes);

  // Install Internet Stack
  InternetStackHelper stack;
  stack.Install(nodes);

  // Assign IP Addresses
  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  // UDP Echo Server on node 1
  uint16_t echoPort = 9;
  UdpEchoServerHelper echoServer(echoPort);
  ApplicationContainer serverApps = echoServer.Install(nodes.Get(1));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  // UDP Echo Client on node 0
  UdpEchoClientHelper echoClient(interfaces.GetAddress(1), echoPort);
  echoClient.SetAttribute("MaxPackets", UintegerValue(5));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  echoClient.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  // Tracing
  traceFile.open("udp-echo.tr");

  Ptr<Queue<Packet>> queue = DynamicCast<Queue<Packet>>(devices.Get(0)->GetObject<PointToPointNetDevice>()->GetQueue());
  if (!queue)
  {
    Ptr<Object> obj = devices.Get(0)->GetObject<Object>();
    queue = obj->GetObject<Queue<Packet>>();
  }
  if (queue)
  {
    queue->TraceConnectWithoutContext("Enqueue", MakeCallback(&QueueEnqueueTrace));
    queue->TraceConnectWithoutContext("Dequeue", MakeCallback(&QueueDequeueTrace));
  }
  // Fallback for CsmaNetDevice (since GetQueue above is specific for point-to-point)
  Ptr<CsmaNetDevice> csmaDev0 = DynamicCast<CsmaNetDevice>(devices.Get(0));
  if (csmaDev0)
  {
    Ptr<Queue<Packet>> csmaQueue = csmaDev0->GetQueue();
    if (csmaQueue)
    {
      csmaQueue->TraceConnectWithoutContext("Enqueue", MakeCallback(&QueueEnqueueTrace));
      csmaQueue->TraceConnectWithoutContext("Dequeue", MakeCallback(&QueueDequeueTrace));
    }
  }

  // Packet reception tracing
  for (uint32_t i = 0; i < devices.GetN(); ++i)
  {
    devices.Get(i)->TraceConnectWithoutContext("MacRx", MakeCallback(&PacketRxTrace));
  }

  // Enable PCAP tracing
  csma.EnablePcapAll("udp-echo", false);

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  traceFile.close();

  return 0;
}