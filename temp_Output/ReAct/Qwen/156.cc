#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/tcp-header.h"
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("StarTopologyDataset");

class PacketLogger {
public:
  static void LogPacket(Ptr<const Packet> packet, const Address &from, const Address &to, uint32_t srcNode, uint32_t dstNode, std::ofstream &csvFile) {
    Time now = Simulator::Now();
    uint32_t packetSize = packet->GetSize();
    csvFile << srcNode << "," << dstNode << "," << packetSize << "," << now.GetSeconds() << "," << now.GetSeconds() << std::endl;
  }
};

int main(int argc, char *argv[]) {
  CommandLine cmd(__FILE__);
  cmd.Parse(argc, argv);

  Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpNewReno::GetTypeId()));

  NodeContainer nodes;
  nodes.Create(5);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("1ms"));

  NetDeviceContainer centralDevices;
  NetDeviceContainer peripheralDevices[4];

  for (uint32_t i = 0; i < 4; ++i) {
    NetDeviceContainer link = p2p.Install(nodes.Get(0), nodes.Get(i + 1));
    centralDevices.Add(link.Get(0));
    peripheralDevices[i].Add(link.Get(1));
  }

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  Ipv4InterfaceContainer interfaces[5];

  address.SetBase("10.1.0.0", "255.255.255.0");
  for (uint32_t i = 0; i < 4; ++i) {
    interfaces[i] = address.Assign(p2p.Install(nodes.Get(0), nodes.Get(i + 1)));
    address.NewNetwork();
  }

  uint16_t sinkPort = 8080;

  PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
  ApplicationContainer sinkApps[4];

  for (uint32_t i = 0; i < 4; ++i) {
    sinkApps[i] = packetSinkHelper.Install(nodes.Get(i + 1));
    sinkApps[i].Start(Seconds(0.0));
    sinkApps[i].Stop(Seconds(10.0));
  }

  OnOffHelper onoff("ns3::TcpSocketFactory", Address());
  onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  onoff.SetAttribute("DataRate", DataRateValue(DataRate("1Mbps")));
  onoff.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer sourceApps[4];

  for (uint32_t i = 0; i < 4; ++i) {
    onoff.SetAttribute("Remote", AddressValue(InetSocketAddress(interfaces[i].GetAddress(1), sinkPort)));
    sourceApps[i] = onoff.Install(nodes.Get(0));
    sourceApps[i].Start(Seconds(1.0 + i));
    sourceApps[i].Stop(Seconds(9.0));
  }

  std::ofstream csvFile;
  csvFile.open("star_topology_dataset.csv");
  csvFile << "SourceNode,DestinationNode,PacketSize,TransmissionTime,ReceptionTime\n";
  csvFile.close();

  csvFile.open("star_topology_dataset.csv", std::ios_base::app);

  Config::Connect("/NodeList/*/ApplicationList/*/$ns3::PacketSink/Rx",
                  MakeBoundCallback(&PacketLogger::LogPacket, std::ref(csvFile)));

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  csvFile.close();
  return 0;
}