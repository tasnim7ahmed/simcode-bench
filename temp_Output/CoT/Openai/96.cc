#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/queue.h"
#include "ns3/packet.h"
#include "ns3/real-time-simulator-impl.h"

using namespace ns3;

void
QueueEnqueueTrace(std::string context, Ptr<const Packet> packet)
{
  static std::ofstream out("udp-echo.tr", std::ios::app);
  out << Simulator::Now().GetSeconds() << " " << context << " Enqueue " << packet->GetUid() << std::endl;
}

void
QueueDequeueTrace(std::string context, Ptr<const Packet> packet)
{
  static std::ofstream out("udp-echo.tr", std::ios::app);
  out << Simulator::Now().GetSeconds() << " " << context << " Dequeue " << packet->GetUid() << std::endl;
}

void
QueueDropTrace(std::string context, Ptr<const Packet> packet)
{
  static std::ofstream out("udp-echo.tr", std::ios::app);
  out << Simulator::Now().GetSeconds() << " " << context << " Drop " << packet->GetUid() << std::endl;
}

void
PacketReceptionTrace(std::string context, Ptr<const Packet> packet, const Address &address)
{
  static std::ofstream out("udp-echo.tr", std::ios::app);
  out << Simulator::Now().GetSeconds() << " " << context << " Receive " << packet->GetUid() << std::endl;
}

int
main(int argc, char *argv[])
{
  LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

  // Use real-time simulator
  GlobalValue::Bind("SimulatorImplementationType", StringValue("ns3::RealtimeSimulatorImpl"));

  NodeContainer nodes;
  nodes.Create(4);

  CsmaHelper csma;
  csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
  csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));
  csma.SetQueue("ns3::DropTailQueue", "MaxPackets", UintegerValue(100));

  NetDeviceContainer devices = csma.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  uint16_t echoPort = 9;

  UdpEchoServerHelper echoServer(echoPort);
  ApplicationContainer serverApps = echoServer.Install(nodes.Get(1));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  UdpEchoClientHelper echoClient(interfaces.GetAddress(1), echoPort);
  echoClient.SetAttribute("MaxPackets", UintegerValue(5));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  echoClient.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  // Tracing queues for each CSMA net device
  for (uint32_t i = 0; i < devices.GetN(); ++i)
    {
      Ptr<NetDevice> dev = devices.Get(i);
      Ptr<CsmaNetDevice> csmaDev = DynamicCast<CsmaNetDevice>(dev);
      if (csmaDev)
        {
          Ptr<Queue<Packet>> queue = csmaDev->GetQueue();
          if (!queue) continue;
          std::ostringstream oss;
          oss << "/NodeList/" << csmaDev->GetNode()->GetId() << "/DeviceList/" << csmaDev->GetIfIndex()
              << "/$ns3::CsmaNetDevice/TxQueue/";
          Config::Connect(oss.str() + "Enqueue", MakeCallback(&QueueEnqueueTrace));
          Config::Connect(oss.str() + "Dequeue", MakeCallback(&QueueDequeueTrace));
          Config::Connect(oss.str() + "Drop", MakeCallback(&QueueDropTrace));
        }
    }

  // Packet reception tracing on node 1 (server)
  devices.Get(1)->TraceConnect("PhyRxEnd", std::string(), MakeBoundCallback(&PacketReceptionTrace));

  // Enable pcap tracing
  csma.EnablePcapAll("udp-echo", false);

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}