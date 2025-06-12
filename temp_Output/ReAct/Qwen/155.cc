#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/tcp-header.h"
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("StarTopologyTcpDataset");

class PacketLogger {
public:
  static void LogPacket(Ptr<const Packet> packet, const Address &from, const Address &to, uint64_t timeSent, uint64_t timeReceived) {
    Ipv4Header ipHeader;
    TcpHeader tcpHdr;
    uint32_t srcNode = 0, dstNode = 0;
    uint16_t packetSize = packet->GetSize();

    packet->PeekHeader(ipHeader);
    packet->PeekHeader(tcpHdr);

    Ptr<Node> node = NodeList::GetNode(0); // dummy to find node IDs
    for (uint32_t i = 0; i < NodeList::GetNNodes(); ++i) {
      Ptr<Node> n = NodeList::GetNode(i);
      for (uint32_t j = 0; j < n->GetObject<Ipv4>()->GetNInterfaces(); ++j) {
        if (ipHeader.GetSource() == n->GetObject<Ipv4>()->GetAddress(j, 0).GetLocal()) {
          srcNode = i;
        }
        if (ipHeader.GetDestination() == n->GetObject<Ipv4>()->GetAddress(j, 0).GetLocal()) {
          dstNode = i;
        }
      }
    }

    *m_os << srcNode << "," << dstNode << "," << packetSize << "," << timeSent << "," << timeReceived << std::endl;
  }

  static void SetupLogStream(const std::string &filename) {
    m_os = new std::ofstream(filename);
    *m_os << "source_node,destination_node,packet_size,transmission_time,reception_time" << std::endl;
  }

  static void CloseLogStream() {
    if (m_os) {
      m_os->close();
      delete m_os;
      m_os = nullptr;
    }
  }

private:
  static std::ofstream *m_os;
};

std::ofstream *PacketLogger::m_os = nullptr;

int main(int argc, char *argv[]) {
  CommandLine cmd(__FILE__);
  cmd.Parse(argc, argv);

  Packet::EnablePrinting();

  NodeContainer nodes;
  nodes.Create(5);

  NodeContainer centralNode = NodeContainer(nodes.Get(0));
  NodeContainer peripheralNodes;
  for (uint32_t i = 1; i <= 4; ++i) {
    peripheralNodes.Add(nodes.Get(i));
  }

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer devices[4];
  for (uint32_t i = 0; i < 4; ++i) {
    devices[i] = p2p.Install(centralNode.Get(0), peripheralNodes.Get(i));
  }

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  Ipv4InterfaceContainer interfaces[4];

  for (uint32_t i = 0; i < 4; ++i) {
    std::ostringstream subnet;
    subnet << "10.1." << i << ".0";
    address.SetBase(subnet.str().c_str(), "255.255.255.0");
    interfaces[i] = address.Assign(devices[i]);
  }

  uint16_t port = 5000;
  PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));

  ApplicationContainer sinks[4];
  for (uint32_t i = 0; i < 4; ++i) {
    sinks[i] = sinkHelper.Install(peripheralNodes.Get(i));
    sinks[i].Start(Seconds(0.0));
    sinks[i].Stop(Seconds(10.0));
  }

  OnOffHelper onoff("ns3::TcpSocketFactory", Address());
  onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  onoff.SetAttribute("DataRate", DataRateValue(DataRate("100Kbps")));
  onoff.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer apps[4];
  for (uint32_t i = 0; i < 4; ++i) {
    AddressValue remoteAddress(InetSocketAddress(interfaces[i].GetAddress(1), port));
    onoff.SetAttribute("Remote", remoteAddress);
    apps[i] = onoff.Install(centralNode);
    apps[i].Start(Seconds(1.0 + i * 0.5));
    apps[i].Stop(Seconds(9.0));
  }

  Config::Connect("/NodeList/*/ApplicationList/*/$ns3::PacketSink/Rx", MakeCallback(&PacketLogger::LogPacket));

  PacketLogger::SetupLogStream("star_topology_tcp_dataset.csv");

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  PacketLogger::CloseLogStream();

  return 0;
}