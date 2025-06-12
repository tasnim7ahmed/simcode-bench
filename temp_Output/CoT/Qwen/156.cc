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
  PacketLogger(const std::string& filename) : m_file(filename) {}

  void LogPacket(Ptr<const Packet> packet, const Address& src, const Address& dst, Time txTime, Time rxTime) {
    Ipv4Header ipHeader;
    packet->PeekHeader(ipHeader);
    uint32_t size = packet->GetSize();

    m_file << InetSocketAddress::ConvertFrom(src).GetIpv4() << ","
           << InetSocketAddress::ConvertFrom(dst).GetIpv4() << ","
           << size << ","
           << txTime.GetSeconds() << ","
           << rxTime.GetSeconds() << std::endl;
  }

private:
  std::ofstream m_file;
};

int main(int argc, char *argv[]) {
  CommandLine cmd(__FILE__);
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(5); // node 0 is the center

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer centralDevice;
  for (uint32_t i = 1; i <= 4; ++i) {
    NetDeviceContainer devices = p2p.Install(nodes.Get(0), nodes.Get(i));
    if (i == 1) {
      centralDevice = devices;
    }
  }

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  for (uint32_t i = 0; i < 4; ++i) {
    std::ostringstream subnet;
    subnet << "10.1." << i << ".0";
    address.SetBase(subnet.str().c_str(), "255.255.255.0");
    address.Assign(p2p.GetNetDevices(nodes.Get(0), nodes.Get(i + 1)));
  }

  PacketLogger logger("star_topology_dataset.csv");
  Config::Connect("/NodeList/*/ApplicationList/*/$ns3::Socket::Tx", MakeCallback(&PacketLogger::LogPacket, &logger));
  Config::Connect("/NodeList/*/DeviceList/*/$ns3::PointToPointNetDevice/MacRx", MakeCallback([=](Ptr<const Packet> packet, const Address& src) {
    static std::map<uint64_t, Time> rxMap;
    uint64_t hash = packet->GetUid();
    if (rxMap.find(hash) == rxMap.end()) {
      rxMap[hash] = Simulator::Now();
    }
  }));

  uint16_t port = 5000;
  for (uint32_t i = 1; i <= 4; ++i) {
    InetSocketAddress sinkAddr(nodes.Get(i)->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal(), port);
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkAddr);
    ApplicationContainer sinkApp = sinkHelper.Install(nodes.Get(i));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(10.0));

    OnOffHelper onoff("ns3::TcpSocketFactory", sinkAddr);
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff.SetAttribute("DataRate", DataRateValue(DataRate("1Mbps")));
    onoff.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApp = onoff.Install(nodes.Get(0));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(10.0));
  }

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}