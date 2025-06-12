#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/udp-echo-helper.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/queue.h"
#include "ns3/drop-tail-queue.h"
#include "ns3/packet.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/realtime-simulator-impl.h"

using namespace ns3;

void
QueueTrace(std::string context, Ptr<const QueueItem> item)
{
  Ptr<Packet> pkt = item->GetPacket();
  NS_LOG_UNCOND(Simulator::Now().GetSeconds() << "s " << context << " Queue packet UID: " << pkt->GetUid() << ", size: " << pkt->GetSize());
}

void
RxTrace(std::string context, Ptr<const Packet> packet, const Address &address)
{
  NS_LOG_UNCOND(Simulator::Now().GetSeconds() << "s " << context << " Packet received. UID: " << packet->GetUid() << ", Size: " << packet->GetSize());
}

int
main(int argc, char *argv[])
{
  // Enable real-time simulator implementation
  GlobalValue::Bind("SimulatorImplementationType",
    StringValue("ns3::RealtimeSimulatorImpl"));

  // Enable output for logging
  LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

  // Create nodes
  NodeContainer nodes;
  nodes.Create(4);

  // CSMA LAN setup
  CsmaHelper csma;
  csma.SetChannelAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
  csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(1)));
  csma.SetQueue("ns3::DropTailQueue<Packet>", "MaxPackets", UintegerValue(100));

  NetDeviceContainer devices = csma.Install(nodes);

  // Install Internet Stack
  InternetStackHelper stack;
  stack.Install(nodes);

  // Assign IP addresses
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

  // Enable pcap tracing
  csma.EnablePcapAll("udp-echo", false);

  // Queue tracing
  Ptr<NetDevice> dev = devices.Get(0);
  Ptr<CsmaNetDevice> csmaDev = DynamicCast<CsmaNetDevice>(dev);
  if (csmaDev)
    {
      Ptr<Queue<Packet>> queue = csmaDev->GetQueue();
      if (queue)
        {
          queue->TraceConnect("Enqueue", "csmaDev0", MakeBoundCallback(&QueueTrace, "csmaDev0"));
          queue->TraceConnect("Dequeue", "csmaDev0", MakeBoundCallback(&QueueTrace, "csmaDev0"));
          queue->TraceConnect("Drop", "csmaDev0", MakeBoundCallback(&QueueTrace, "csmaDev0"));
        }
    }

  // Packet reception tracing (UDP echo server Rx)
  Ptr<Node> serverNode = nodes.Get(1);
  Ptr<Ipv4> ipv4 = serverNode->GetObject<Ipv4>();
  uint32_t interface = 1; // first csma interface
  Ipv4Address serverAddr = interfaces.GetAddress(1);

  for (uint32_t i = 0; i < serverNode->GetNDevices(); ++i)
    {
      Ptr<NetDevice> netDev = serverNode->GetDevice(i);
      if (DynamicCast<CsmaNetDevice>(netDev))
        {
          netDev->TraceConnect("MacRx", "ServerMacRx", MakeBoundCallback(&RxTrace, "ServerMacRx"));
        }
    }

  // Output tracing to file
  std::ofstream traceFile("udp-echo.tr");
  if (!traceFile)
    {
      NS_FATAL_ERROR("Could not open udp-echo.tr for writing");
    }

  // Redirect all NS_LOG_UNCOND to our file
  LogComponentEnableAll(LOG_PREFIX_TIME);

  class TraceRedirect : public std::ostream
  {
  public:
    TraceRedirect(std::ostream& o) : std::ostream(o.rdbuf()) {}
    ~TraceRedirect() { std::cout.rdbuf(nullptr); }
  };

  TraceRedirect redirect(traceFile);
  std::streambuf *coutbuf = std::cout.rdbuf();
  std::cout.rdbuf(traceFile.rdbuf());

  Simulator::Stop(Seconds(11.0));
  Simulator::Run();
  Simulator::Destroy();

  std::cout.rdbuf(coutbuf);
  traceFile.close();

  return 0;
}