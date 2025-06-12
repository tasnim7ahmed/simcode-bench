#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"
#include "ns3/mobility-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("RingTopologyUdpDataset");

class PacketLogger
{
public:
  static void LogPacket(Ptr<Socket> socket, Ptr<const Packet> packet, const Address& address)
  {
    if (InetSocketAddress::IsMatchingType(address))
    {
      Ipv4Address destIp = InetSocketAddress::ConvertFrom(address).GetIpv4();
      uint16_t destPort = InetSocketAddress::ConvertFrom(address).GetPort();

      // Find destination node ID by IP
      int destNodeId = -1;
      for (int i = 0; i < 4; ++i)
      {
        if (destIp == Ipv4Address(("10.1.1." + std::to_string(i + 1)).c_str()))
        {
          destNodeId = i;
          break;
        }
      }

      if (destNodeId == -1) return;

      // Get source node from socket
      Ipv4Address srcIp = socket->GetLocalAddress().ConvertTo<Ipv4Address>();
      int srcNodeId = -1;
      for (int i = 0; i < 4; ++i)
      {
        if (srcIp == Ipv4Address(("10.1.1." + std::to_string(i + 1)).c_str()))
        {
          srcNodeId = i;
          break;
        }
      }

      if (srcNodeId == -1) return;

      double txTime = Simulator::Now().GetSeconds();
      double rxTime = txTime; // Placeholder for receive time

      *g_outputStream << srcNodeId << ","
                      << destNodeId << ","
                      << packet->GetSize() << ","
                      << txTime << ","
                      << rxTime << std::endl;
    }
  }

  static void LogRx(Ptr<const Packet> packet, const Address& from, const Address& to, uint16_t port, uint32_t bytes)
  {
    Ipv4Address destIp = InetSocketAddress::ConvertFrom(to).GetIpv4();
    uint16_t destPort = InetSocketAddress::ConvertFrom(to).GetPort();

    int destNodeId = -1;
    for (int i = 0; i < 4; ++i)
    {
      if (destIp == Ipv4Address(("10.1.1." + std::to_string(i + 1)).c_str()))
      {
        destNodeId = i;
        break;
      }
    }

    if (destNodeId == -1) return;

    Ipv4Address srcIp = InetSocketAddress::ConvertFrom(from).GetIpv4();
    int srcNodeId = -1;
    for (int i = 0; i < 4; ++i)
    {
      if (srcIp == Ipv4Address(("10.1.1." + std::to_string(i + 1)).c_str()))
      {
        srcNodeId = i;
        break;
      }
    }

    if (srcNodeId == -1) return;

    double txTime = Simulator::Now().GetSeconds(); // Actually this is the receive timestamp
    double rxTime = txTime;

    *g_outputStream << srcNodeId << ","
                    << destNodeId << ","
                    << bytes << ","
                    << txTime << ","
                    << rxTime << std::endl;
  }
};

std::ofstream* g_outputStream = nullptr;

int main(int argc, char* argv[])
{
  CommandLine cmd(__FILE__);
  cmd.Parse(argc, argv);

  g_outputStream = new std::ofstream("ring_topology_dataset.csv");
  *g_outputStream << "source_node,destination_node,packet_size,transmission_time,reception_time" << std::endl;

  NodeContainer nodes;
  nodes.Create(4);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer devices[4];
  for (uint32_t i = 0; i < 4; ++i)
  {
    devices[i] = p2p.Install(nodes.Get(i), nodes.Get((i + 1) % 4));
  }

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  for (uint32_t i = 0; i < 4; ++i)
  {
    std::string network = "10.1." + std::to_string(i) + ".0";
    address.SetBase(network.c_str(), "255.255.255.0");
    address.Assign(devices[i]);
  }

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  UdpEchoServerHelper echoServer(9);
  ApplicationContainer serverApps = echoServer.Install(nodes.Get(1));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  UdpEchoClientHelper echoClient(nodes.Get(1)->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal(), 9);
  echoClient.SetAttribute("MaxPackets", UintegerValue(20));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  echoClient.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  // Install packet logging
  Config::Connect("/NodeList/*/ApplicationList/*/$ns3::UdpEchoClient/Tx", MakeCallback(&PacketLogger::LogPacket));
  Config::Connect("/NodeList/*/ApplicationList/*/$ns3::UdpEchoServer/Rx", MakeCallback(&PacketLogger::LogRx));

  Simulator::Run();
  Simulator::Destroy();

  delete g_outputStream;

  return 0;
}