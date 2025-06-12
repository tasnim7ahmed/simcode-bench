#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("RingTopologyUdpDataset");

class UdpPacketLogger
{
public:
  UdpPacketLogger(const std::string& filename);
  void LogPacket(Ptr<const Packet> packet, const Address& srcAddr, const Address& dstAddr, Time txTime);

private:
  std::ofstream m_ofs;
};

UdpPacketLogger::UdpPacketLogger(const std::string& filename)
{
  m_ofs.open(filename.c_str());
  m_ofs << "Source,Destination,Size,TransmissionTime,ReceptionTime\n";
}

void
UdpPacketLogger::LogPacket(Ptr<const Packet> packet, const Address& srcAddr, const Address& dstAddr, Time txTime)
{
  Ipv4Address srcIp = InetSocketAddress::ConvertFrom(srcAddr).GetIpv4();
  Ipv4Address dstIp = InetSocketAddress::ConvertFrom(dstAddr).GetIpv4();
  uint32_t size = packet->GetSize();
  Time rxTime = Simulator::Now();

  m_ofs << srcIp << "," << dstIp << "," << size << "," << txTime.GetSeconds() << "," << rxTime.GetSeconds() << "\n";
}

int
main(int argc, char* argv[])
{
  CommandLine cmd(__FILE__);
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(4);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer devices[4];
  for (uint32_t i = 0; i < 4; ++i)
    {
      uint32_t j = (i + 1) % 4;
      devices[i] = p2p.Install(nodes.Get(i), nodes.Get(j));
    }

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  Ipv4InterfaceContainer interfaces[4];
  for (uint32_t i = 0; i < 4; ++i)
    {
      std::ostringstream subnet;
      subnet << "10.1." << i << ".0";
      address.SetBase(subnet.str().c_str(), "255.255.255.0");
      interfaces[i] = address.Assign(devices[i]);
    }

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  UdpEchoServerHelper echoServer(9);

  ApplicationContainer serverApps = echoServer.Install(nodes.Get(1));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  UdpEchoClientHelper echoClient(interfaces[0].GetAddress(1), 9);
  echoClient.SetAttribute("MaxPackets", UintegerValue(20));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(0.5)));
  echoClient.SetAttribute("PacketSize", UintegerValue(512));

  ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  UdpPacketLogger logger("ring_topology_udp_packets.csv");

  Config::Connect("/NodeList/*/ApplicationList/*/$ns3::UdpEchoClient/Tx", MakeCallback(&UdpPacketLogger::LogPacket, &logger));

  Simulator::Stop(Seconds(11.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}