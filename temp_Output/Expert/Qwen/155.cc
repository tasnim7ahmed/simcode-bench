#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/tcp-header.h"
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("StarTopologyTcpDataset");

class StarTopologyTcpDataset {
public:
  StarTopologyTcpDataset();
  void Run();
private:
  void SetupNodes();
  void SetupDevices();
  void SetupStack();
  void SetupApplications();
  void SetupCallbacks();
  void ScheduleTeardown();
  void PacketSent(Ptr<const Packet> packet, const TcpHeader& header, Socket::SocketErrno errno_);
  void PacketReceived(Socket::Ptr socket);
  void WriteToFile();

  NodeContainer m_nodes;
  NetDeviceContainer m_devices[5];
  InternetStackHelper m_stack;
  Ipv4InterfaceContainer m_interfaces[5];
  std::ofstream m_csvFile;
  std::vector<std::tuple<uint32_t, uint32_t, uint32_t, double, double>> m_dataset;
};

StarTopologyTcpDataset::StarTopologyTcpDataset() {
  m_csvFile.open("star_topology_tcp_dataset.csv");
  m_csvFile << "Source,Destination,Size,TransmissionTime,ReceptionTime\n";
}

void
StarTopologyTcpDataset::SetupNodes() {
  m_nodes.Create(5);
}

void
StarTopologyTcpDataset::SetupDevices() {
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("1ms"));

  for (uint32_t i = 1; i <= 4; ++i) {
    NetDeviceContainer dev = p2p.Install(m_nodes.Get(0), m_nodes.Get(i));
    m_devices[i-1] = dev;
  }
}

void
StarTopologyTcpDataset::SetupStack() {
  m_stack.Install(m_nodes);
  Ipv4AddressHelper address;
  for (uint32_t i = 1; i <= 4; ++i) {
    address.SetBase("10.1." + std::to_string(i) + ".0", "255.255.255.0");
    m_interfaces[i-1] = address.Assign(m_devices[i-1]);
  }
}

void
StarTopologyTcpDataset::SetupApplications() {
  uint16_t port = 50000;
  for (uint32_t i = 1; i <= 4; ++i) {
    Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
    ApplicationContainer sinkApp = sinkHelper.Install(m_nodes.Get(i));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(10.0));

    AddressValue remoteAddress(InetSocketAddress(m_interfaces[i-1].GetAddress(1), port));
    Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(1448));
    OnOffHelper onoff("ns3::TcpSocketFactory", Address());
    onoff.SetAttribute("Remote", remoteAddress);
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff.SetAttribute("DataRate", DataRateValue(DataRate("1Mbps")));
    onoff.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApp = onoff.Install(m_nodes.Get(0));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(9.0));
  }
}

void
StarTopologyTcpDataset::SetupCallbacks() {
  Config::Connect("/NodeList/*/ApplicationList/*/$ns3::PacketSink/Rx",
                  MakeCallback(&StarTopologyTcpDataset::PacketReceived, this));
  Config::Connect("/NodeList/0/ApplicationList/0/$ns3::OnOffApplication/Tx",
                  MakeCallback(&StarTopologyTcpDataset::PacketSent, this));
}

void
StarTopologyTcpDataset::ScheduleTeardown() {
  Simulator::Schedule(Seconds(10.0), &StarTopologyTcpDataset::WriteToFile, this);
  Simulator::Schedule(Seconds(10.0), &Simulator::Stop, &Simulator::GetReference());
}

void
StarTopologyTcpDataset::PacketSent(Ptr<const Packet> packet, const TcpHeader& header, Socket::SocketErrno errno_) {
  uint32_t size = packet->GetSize();
  double txTime = Simulator::Now().GetSeconds();
  m_dataset.emplace_back(0, header.GetDestinationPort() - 50000 + 1, size, txTime, 0.0); // Store dummy reception time
}

void
StarTopologyTcpDataset::PacketReceived(Socket::Ptr socket) {
  Ptr<Packet> packet;
  while ((packet = socket->Recv())) {
    uint32_t size = packet->GetSize();
    double rxTime = Simulator::Now().GetSeconds();
    for (auto& entry : m_dataset) {
      if (std::get<4>(entry) == 0.0 && std::get<2>(entry) == size) {
        std::get<4>(entry) = rxTime;
        break;
      }
    }
  }
}

void
StarTopologyTcpDataset::WriteToFile() {
  for (const auto& record : m_dataset) {
    m_csvFile << std::get<0>(record) << ","
              << std::get<1>(record) << ","
              << std::get<2>(record) << ","
              << std::fixed << std::setprecision(9) << std::get<3>(record) << ","
              << std::fixed << std::setprecision(9) << std::get<4>(record) << "\n";
  }
  m_csvFile.close();
}

void
StarTopologyTcpDataset::Run() {
  SetupNodes();
  SetupDevices();
  SetupStack();
  SetupApplications();
  SetupCallbacks();
  ScheduleTeardown();
  Simulator::Run();
  Simulator::Destroy();
}

int
main(int argc, char* argv[]) {
  StarTopologyTcpDataset simulation;
  simulation.Run();
  return 0;
}