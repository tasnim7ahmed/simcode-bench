#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/tcp-header.h"
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("StarTopologyDataset");

class StarTopologySim {
public:
  void Run();
private:
  void SetupNodes();
  void SetupDevices();
  void SetupStack();
  void InstallApplications();
  void PacketSent(Ptr<const Packet> packet, const TcpHeader& header, Ptr<const Socket> socket);
  void PacketReceived(Ptr<const Packet> packet, const Address &from);

  NodeContainer centralNode;
  NodeContainer peripheralNodes;
  NetDeviceContainer centralDevice;
  NetDeviceContainer peripheralDevices;
  Ipv4InterfaceContainer centralInterface;
  Ipv4InterfaceContainer peripheralInterfaces;
  std::ofstream m_csvFile;
};

void StarTopologySim::Run() {
  SetupNodes();
  SetupDevices();
  SetupStack();
  InstallApplications();

  m_csvFile.open("star_topology_packets.csv");
  m_csvFile << "Source,Destination,Size,TransmissionTime,ReceptionTime\n";

  Config::Connect("/NodeList/*/ApplicationList/0/$ns3::TcpSocketBase/Tx", MakeCallback(&StarTopologySim::PacketSent, this));
  Config::Connect("/NodeList/*/DeviceList/0/$ns3::PointToPointNetDevice/MacRx", MakeCallback(&StarTopologySim::PacketReceived, this));

  Simulator::Run();
  m_csvFile.close();
  Simulator::Destroy();
}

void StarTopologySim::SetupNodes() {
  centralNode.Create(1);
  peripheralNodes.Create(4);
}

void StarTopologySim::SetupDevices() {
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));

  for (uint32_t i = 0; i < peripheralNodes.GetN(); ++i) {
    NetDeviceContainer devices = p2p.Install(centralNode.Get(0), peripheralNodes.Get(i));
    centralDevice.Add(devices.Get(0));
    peripheralDevices.Add(devices.Get(1));
  }
}

void StarTopologySim::SetupStack() {
  InternetStackHelper stack;
  stack.Install(centralNode);
  stack.Install(peripheralNodes);

  Ipv4AddressHelper address;
  for (uint32_t i = 0; i < peripheralNodes.GetN(); ++i) {
    std::ostringstream subnet;
    subnet << "10.1." << i << ".0";
    address.SetBase(subnet.str().c_str(), "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(NetDeviceContainer(centralDevice.Get(i), peripheralDevices.Get(i)));
    centralInterface.Add(interfaces.Get(0));
    peripheralInterfaces.Add(interfaces.Get(1));
  }
}

void StarTopologySim::InstallApplications() {
  uint16_t port = 5000;
  for (uint32_t i = 0; i < peripheralNodes.GetN(); ++i) {
    InetSocketAddress sinkAddr(centralInterface.GetAddress(0), port++);
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkAddr);
    ApplicationContainer sinkApp = sinkHelper.Install(peripheralNodes.Get(i));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(10.0));

    OnOffHelper onoff("ns3::TcpSocketFactory", sinkAddr);
    onoff.SetConstantRate(DataRate("1kbps"));
    onoff.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer app = onoff.Install(centralNode.Get(0));
    app.Start(Seconds(1.0 + i * 0.5));
    app.Stop(Seconds(9.0));
  }
}

void StarTopologySim::PacketSent(Ptr<const Packet> packet, const TcpHeader& header, Ptr<const Socket> socket) {
  Ipv4Header ipHeader;
  packet->PeekHeader(ipHeader);
  Ipv4Address sourceIp = ipHeader.GetSource();
  Ipv4Address destIp = ipHeader.GetDestination();

  int srcId = -1, dstId = -1;
  for (uint32_t i = 0; i < centralInterface.GetN(); ++i) {
    if (centralInterface.GetAddress(i) == sourceIp) { srcId = 0; dstId = i + 1; break; }
    if (centralInterface.GetAddress(i) == destIp) { dstId = 0; srcId = i + 1; break; }
  }

  if (srcId != -1 && dstId != -1) {
    m_csvFile << srcId << "," << dstId << "," << packet->GetSize() << "," << Simulator::Now().GetSeconds() << ",";
  }
}

void StarTopologySim::PacketReceived(Ptr<const Packet> packet, const Address &from) {
  m_csvFile << Simulator::Now().GetSeconds() << "\n";
}

int main(int argc, char *argv[]) {
  StarTopologySim sim;
  sim.Run();
  return 0;
}