#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("RingTopologyUdpDataset");

class UdpPacketLogger {
public:
  UdpPacketLogger(const std::string &filename) : m_ofs(filename.c_str()) {
    m_ofs << "Source,Destination,Size,TransmissionTime,ReceptionTime\n";
  }

  void LogTx(Ptr<const Packet> packet, const Address &dest) {
    auto it = m_pendingPackets.find(std::make_pair(Address::ConvertFrom(dest), packet->GetUid()));
    if (it == m_pendingPackets.end()) {
      m_pendingPackets[std::make_pair(Address::ConvertFrom(dest), packet->GetUid())] = Simulator::Now();
    }
  }

  void LogRx(Ptr<const Packet> packet, const Address &src) {
    auto keyPair = std::make_pair(Address::ConvertFrom(src), packet->GetUid());
    auto it = m_pendingPackets.find(keyPair);
    if (it != m_pendingPackets.end()) {
      Time txTime = it->second;
      m_ofs << src << "," << PacketSocketAddress::ConvertFrom(keyPair.first).GetPhysicalAddress()
            << "," << packet->GetSize() << "," << txTime.GetSeconds() << ","
            << Simulator::Now().GetSeconds() << "\n";
      m_pendingPackets.erase(it);
    }
  }

private:
  std::ofstream m_ofs;
  std::map<std::pair<Address, uint64_t>, Time> m_pendingPackets;
};

int main(int argc, char *argv[]) {
  std::string csvFile = "ring_topology_dataset.csv";

  CommandLine cmd(__FILE__);
  cmd.AddValue("csvFile", "Name of CSV output file", csvFile);
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(4);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer devices[4];
  for (uint32_t i = 0; i < 4; ++i) {
    uint32_t j = (i + 1) % 4;
    devices[i] = p2p.Install(nodes.Get(i), nodes.Get(j));
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

  UdpEchoServerHelper echoServer(9);
  ApplicationContainer serverApps = echoServer.Install(nodes.Get(1));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  UdpEchoClientHelper echoClient(interfaces[1].GetAddress(1), 9);
  echoClient.SetAttribute("MaxPackets", UintegerValue(20));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  echoClient.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  UdpPacketLogger logger(csvFile);

  Config::Connect("/NodeList/*/ApplicationList/*/$ns3::UdpEchoClient/Tx", MakeCallback(&UdpPacketLogger::LogTx, &logger));
  Config::Connect("/NodeList/*/ApplicationList/*/$ns3::UdpEchoServer/Rx", MakeCallback(&UdpPacketLogger::LogRx, &logger));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  Simulator::Run();
  Simulator::Destroy();
  return 0;
}