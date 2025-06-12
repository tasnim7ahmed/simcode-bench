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
  PacketLogger(const std::string& filename) : m_outFile(filename) {
    m_outFile << "Source,Destination,PacketSize,TransmissionTime,ReceptionTime\n";
  }

  void LogTx(Ptr<const Packet> packet, const Address& dest) {
    Ipv4Header ipHeader;
    TcpHeader tcpHeader;
    packet->PeekHeader(ipHeader);
    packet->PeekHeader(tcpHeader);

    uint32_t source = ipHeader.GetSource().Get();
    uint32_t destination = ipHeader.GetDestination().Get();

    m_txMap[packet->GetUid()] = std::make_tuple(source, destination, Simulator::Now(), packet->GetSize());
  }

  void LogRx(Ptr<const Packet> packet) {
    auto it = m_txMap.find(packet->GetUid());
    if (it != m_txMap.end()) {
      uint32_t source = std::get<0>(it->second);
      uint32_t destination = std::get<1>(it->second);
      Time txTime = std::get<2>(it->second);
      uint32_t size = std::get<3>(it->second);
      Time rxTime = Simulator::Now();

      m_outFile << source << "," << destination << "," << size << "," << txTime.GetSeconds() << "," << rxTime.GetSeconds() << "\n";

      m_txMap.erase(it);
    }
  }

private:
  std::ofstream m_outFile;
  std::map<uint64_t, std::tuple<uint32_t, uint32_t, Time, uint32_t>> m_txMap;
};

int main(int argc, char *argv[]) {
  std::string csvFilename = "star_topology_dataset.csv";

  NodeContainer centralNode;
  centralNode.Create(1); // node 0

  NodeContainer peripheralNodes;
  peripheralNodes.Create(4); // nodes 1-4

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer devicesCentral[4];
  NetDeviceContainer devicesPeripheral[4];
  Ipv4InterfaceContainer interfacesCentral[4];
  Ipv4InterfaceContainer interfacesPeripheral[4];

  InternetStackHelper stack;
  stack.InstallAll();

  Ipv4AddressHelper address;
  for (uint32_t i = 0; i < 4; ++i) {
    address.SetBase("10.1." + std::to_string(i) + ".0", "255.255.255.0");
    devicesCentral[i] = p2p.Install(centralNode.Get(0), peripheralNodes.Get(i));
    interfacesCentral[i] = address.Assign(devicesCentral[i]);
    interfacesPeripheral[i] = interfacesCentral[i]; // same devices
  }

  PacketLogger logger(csvFilename);

  Config::ConnectWithoutContext("/NodeList/*/ApplicationList/*/$ns3::TcpSocketSink/Rx", MakeCallback(&PacketLogger::LogRx, &logger));

  uint16_t port = 50000;
  for (uint32_t i = 0; i < 4; ++i) {
    InetSocketAddress sinkAddr(interfacesCentral[i].GetAddress(0), port);
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkAddr);
    ApplicationContainer sinkApp = packetSinkHelper.Install(peripheralNodes.Get(i));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(10.0));

    OnOffHelper onoff("ns3::TcpSocketFactory", sinkAddr);
    onoff.SetConstantRate(DataRate("100kbps"));
    onoff.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer app = onoff.Install(centralNode.Get(0));
    app.Start(Seconds(1.0 + i));
    app.Stop(Seconds(9.0));

    Config::ConnectWithoutContext("/NodeList/" + std::to_string(centralNode.Get(0)->GetId()) + "/ApplicationList/0/$ns3::OnOffApplication/Tx", 
                                  MakeCallback(&PacketLogger::LogTx, &logger));
  }

  Simulator::Run();
  Simulator::Destroy();
  return 0;
}