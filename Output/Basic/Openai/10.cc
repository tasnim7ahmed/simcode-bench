#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv6-address-helper.h"
#include "ns3/applications-module.h"
#include "ns3/packet-socket-helper.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/queue.h"
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("CsmaUdpEchoExample");

void
RxTraceCallback(Ptr<const Packet> packet, const Address &address)
{
  static std::ofstream recvTraceFile;
  static bool initialized = false;
  if (!initialized)
    {
      recvTraceFile.open("udp-echo.tr", std::ios_base::out | std::ios_base::trunc);
      initialized = true;
    }
  recvTraceFile << "Packet received at " << Simulator::Now().GetSeconds()
                << "s, size: " << packet->GetSize() << " bytes" << std::endl;
}

void
EnqueueTrace(std::string context, Ptr<const Packet> packet)
{
  static std::ofstream trFile;
  static bool initialized = false;
  if (!initialized)
    {
      trFile.open("udp-echo.tr", std::ios_base::out | std::ios_base::app);
      initialized = true;
    }
  trFile << "Enqueue at " << Simulator::Now().GetSeconds()
         << "s, size: " << packet->GetSize() << " bytes, context: " << context << std::endl;
}

void
DequeueTrace(std::string context, Ptr<const Packet> packet)
{
  static std::ofstream trFile;
  static bool initialized = false;
  if (!initialized)
    {
      trFile.open("udp-echo.tr", std::ios_base::out | std::ios_base::app);
      initialized = true;
    }
  trFile << "Dequeue at " << Simulator::Now().GetSeconds()
         << "s, size: " << packet->GetSize() << " bytes, context: " << context << std::endl;
}

void
DropTrace(std::string context, Ptr<const Packet> packet)
{
  static std::ofstream trFile;
  static bool initialized = false;
  if (!initialized)
    {
      trFile.open("udp-echo.tr", std::ios_base::out | std::ios_base::app);
      initialized = true;
    }
  trFile << "Drop at " << Simulator::Now().GetSeconds()
         << "s, size: " << packet->GetSize() << " bytes, context: " << context << std::endl;
}

int
main(int argc, char *argv[])
{
  bool enableIpv6 = false;

  CommandLine cmd;
  cmd.AddValue("ipv6", "Enable IPv6 addressing instead of IPv4", enableIpv6);
  cmd.Parse(argc, argv);

  LogComponentEnable("CsmaUdpEchoExample", LOG_LEVEL_INFO);
  LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create(4);

  CsmaHelper csma;
  csma.SetChannelAttribute("DataRate", StringValue("5Mbps"));
  csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
  csma.SetDeviceAttribute("Mtu", UintegerValue(1400));

  NetDeviceContainer devices = csma.Install(nodes);

  InternetStackHelper stack;
  if (enableIpv6)
    {
      stack.SetIpv4StackInstall(false);
      stack.SetIpv6StackInstall(true);
    }
  else
    {
      stack.SetIpv4StackInstall(true);
      stack.SetIpv6StackInstall(false);
    }
  stack.Install(nodes);

  Ipv4AddressHelper ipv4;
  Ipv6AddressHelper ipv6;

  Ipv4InterfaceContainer ipv4Interfaces;
  Ipv6InterfaceContainer ipv6Interfaces;

  if (!enableIpv6)
    {
      ipv4.SetBase("10.1.1.0", "255.255.255.0");
      ipv4Interfaces = ipv4.Assign(devices);
    }
  else
    {
      ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
      ipv6Interfaces = ipv6.Assign(devices);
    }

  uint16_t echoPort = 9;
  UdpEchoServerHelper echoServer(echoPort);

  ApplicationContainer serverApps;
  if (!enableIpv6)
    {
      serverApps = echoServer.Install(nodes.Get(1));
    }
  else
    {
      serverApps = echoServer.Install(nodes.Get(1));
      // The server will listen on both IPv4 and IPv6
    }
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(11.0));

  Address serverAddress;
  if (!enableIpv6)
    {
      serverAddress = InetSocketAddress(ipv4Interfaces.GetAddress(1), echoPort);
    }
  else
    {
      serverAddress = Inet6SocketAddress(ipv6Interfaces.GetAddress(1, 1), echoPort);
    }

  UdpEchoClientHelper echoClient(serverAddress);
  echoClient.SetAttribute("MaxPackets", UintegerValue(1));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  echoClient.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  // Trace queue events
  for (uint32_t i = 0; i < devices.GetN(); ++i)
    {
      Ptr<NetDevice> nd = devices.Get(i);
      Ptr<CsmaNetDevice> csmaNd = DynamicCast<CsmaNetDevice>(nd);
      Ptr<Queue<Packet>> q = csmaNd->GetQueue();
      if (q)
        {
          q->TraceConnect("Enqueue", "queue-dev" + std::to_string(i), MakeCallback(&EnqueueTrace));
          q->TraceConnect("Dequeue", "queue-dev" + std::to_string(i), MakeCallback(&DequeueTrace));
          q->TraceConnect("Drop",    "queue-dev" + std::to_string(i), MakeCallback(&DropTrace));
        }
    }

  // Trace packet receptions at the application socket on n1
  Ptr<Node> node1 = nodes.Get(1);
  Ptr<Application> app = serverApps.Get(0);
  Ptr<UdpEchoServer> echoSrv = DynamicCast<UdpEchoServer>(app);
  if (echoSrv)
    {
      echoSrv->TraceConnectWithoutContext("Rx", MakeCallback(&RxTraceCallback));
    }

  // Enable pcap tracing for all CSMA devices
  csma.EnablePcapAll("udp-echo", true);

  Simulator::Stop(Seconds(12.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}