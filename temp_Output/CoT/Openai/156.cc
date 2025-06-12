#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include <fstream>
#include <sstream>
#include <map>

using namespace ns3;

struct PacketLogEntry {
  uint32_t packetId;
  uint32_t srcNode;
  uint32_t dstNode;
  uint32_t size;
  double txTime;
  double rxTime;
  bool hasTx = false;
  bool hasRx = false;
};

std::map<uint32_t, PacketLogEntry> packetLog;
std::ofstream outFile("star_simulation.csv");

void
TxTrace(Ptr<const Packet> packet, const Address &address, uint32_t srcNode, uint32_t dstNode) {
  uint32_t id = packet->GetUid();
  double time = Simulator::Now().GetSeconds();

  auto &entry = packetLog[id];
  entry.packetId = id;
  entry.srcNode = srcNode;
  entry.dstNode = dstNode;
  entry.size = packet->GetSize();
  entry.txTime = time;
  entry.hasTx = true;

  if (entry.hasRx) {
    outFile << entry.srcNode << "," << entry.dstNode << "," << entry.size << "," << entry.txTime << "," << entry.rxTime << "\n";
    packetLog.erase(id);
  }
}

void
RxTrace(Ptr<const Packet> packet, const Address &address, uint32_t srcNode, uint32_t dstNode) {
  uint32_t id = packet->GetUid();
  double time = Simulator::Now().GetSeconds();

  auto &entry = packetLog[id];
  entry.packetId = id;
  entry.srcNode = srcNode;
  entry.dstNode = dstNode;
  entry.size = packet->GetSize();
  entry.rxTime = time;
  entry.hasRx = true;

  if (entry.hasTx) {
    outFile << entry.srcNode << "," << entry.dstNode << "," << entry.size << "," << entry.txTime << "," << entry.rxTime << "\n";
    packetLog.erase(id);
  }
}

int
main(int argc, char *argv[])
{
  outFile << "src_node,dst_node,packet_size,tx_time,rx_time\n";
  CommandLine cmd;
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(5);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer netDevices[4];
  NodeContainer starPairs[4];
  for (uint32_t i = 0; i < 4; ++i) {
    starPairs[i] = NodeContainer(nodes.Get(0), nodes.Get(i+1));
    netDevices[i] = p2p.Install(starPairs[i]);
  }

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  Ipv4InterfaceContainer interfaces[4];
  for (uint32_t i = 0; i < 4; ++i) {
    std::ostringstream subnet;
    subnet << "10.0." << i+1 << ".0";
    address.SetBase(subnet.str().c_str(), "255.255.255.0");
    interfaces[i] = address.Assign(netDevices[i]);
  }

  uint16_t port = 8080;
  ApplicationContainer sinkApps;
  for (uint32_t i = 1; i <= 4; ++i) {
    Address bindAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
    Ptr<PacketSink> sink = CreateObject<PacketSink>();
    sink->SetAttribute("Local", AddressValue(bindAddress));
    nodes.Get(i)->AddApplication(sink);
    sink->SetStartTime(Seconds(0.0));
    sink->SetStopTime(Seconds(10.0));
    sinkApps.Add(sink);

    std::ostringstream path;
    path << "/NodeList/" << i << "/ApplicationList/" << sink->GetId() << "/$ns3::PacketSink/Rx";
    Config::Connect(path.str(), MakeBoundCallback(&RxTrace, 0, i));
  }

  for (uint32_t i = 1; i <= 4; ++i) {
    OnOffHelper onoff("ns3::TcpSocketFactory", Address(InetSocketAddress(interfaces[i-1].GetAddress(1), port)));
    onoff.SetAttribute("DataRate", StringValue("1Mbps"));
    onoff.SetAttribute("PacketSize", UintegerValue(512));
    onoff.SetAttribute("MaxBytes", UintegerValue(4096));
    onoff.SetAttribute("StartTime", TimeValue(Seconds(1.0 + 0.5*i)));
    onoff.SetAttribute("StopTime", TimeValue(Seconds(9.0)));

    ApplicationContainer app = onoff.Install(nodes.Get(0));
    for (uint32_t j = 0; j < app.GetN(); ++j) {
      Ptr<Application> application = app.Get(j);
      std::ostringstream path;
      path << "/NodeList/0/ApplicationList/" << application->GetId() << "/$ns3::OnOffApplication/Tx";
      Config::Connect(path.str(), MakeBoundCallback(&TxTrace, 0, i));
    }
  }

  Simulator::Stop(Seconds(10.1));
  Simulator::Run();
  outFile.close();
  Simulator::Destroy();
  return 0;
}