#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/tcp-header.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("StarTopologyTcpDataset");

class StarTopologyTcpDataset : public Object
{
public:
  static TypeId GetTypeId(void);
  StarTopologyTcpDataset();
  virtual ~StarTopologyTcpDataset();

private:
  void SetupNodes();
  void SetupDevices();
  void SetupInternet();
  void SetupApplications();
  void SetupTracing();
  void WritePacketDataToFile(Ptr<const Packet> packet, const Address &from, const Address &to, Time txTime, Time rxTime);

  NodeContainer m_nodes;
  NetDeviceContainer m_devices[5]; // node0 connects to nodes1-4
  Ipv4InterfaceContainer m_interfaces[5];
  uint32_t m_packetCount;
  std::ofstream m_outputFile;
};

TypeId
StarTopologyTcpDataset::GetTypeId(void)
{
  static TypeId tid = TypeId("StarTopologyTcpDataset")
    .SetParent<Object>()
    .AddConstructor<StarTopologyTcpDataset>();
  return tid;
}

StarTopologyTcpDataset::StarTopologyTcpDataset()
{
  m_packetCount = 0;
  m_outputFile.open("tcp_dataset.csv");
  m_outputFile << "Source,Destination,Size,TransmissionTime,ReceptionTime\n";
}

StarTopologyTcpDataset::~StarTopologyTcpDataset()
{
  m_outputFile.close();
}

void
StarTopologyTcpDataset::SetupNodes()
{
  m_nodes.Create(5); // node0 is central
}

void
StarTopologyTcpDataset::SetupDevices()
{
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("1ms"));

  for (uint32_t i = 1; i <= 4; ++i)
  {
    NetDeviceContainer devices = p2p.Install(m_nodes.Get(0), m_nodes.Get(i));
    m_devices[i - 1] = devices;
  }
}

void
StarTopologyTcpDataset::SetupInternet()
{
  InternetStackHelper internet;
  internet.Install(m_nodes);

  Ipv4AddressHelper ipv4;
  char subnet[32];
  for (uint32_t i = 0; i < 4; ++i)
  {
    sprintf(subnet, "10.1.%u.0", i);
    ipv4.SetBase(subnet, "255.255.255.0");
    m_interfaces[i] = ipv4.Assign(m_devices[i]);
  }
}

void
StarTopologyTcpDataset::WritePacketDataToFile(Ptr<const Packet> packet, const Address &from, const Address &to, Time txTime, Time rxTime)
{
  Ipv4Header ipHeader;
  TcpHeader tcpHeader;
  uint32_t srcNode = 0;
  uint32_t dstNode = 0;

  if (packet->PeekHeader(ipHeader))
  {
    Ipv4Address srcIp = ipHeader.GetSource();
    Ipv4Address dstIp = ipHeader.GetDestination();

    for (uint32_t i = 0; i < 5; ++i)
    {
      for (uint32_t j = 0; j < 4; ++j)
      {
        if (m_interfaces[j].GetAddress(1) == srcIp)
          srcNode = i + 1;
        if (m_interfaces[j].GetAddress(1) == dstIp)
          dstNode = i + 1;
      }
    }

    if (srcNode == 0 && dstNode == 0)
    {
      // Try checking node 0's addresses
      for (uint32_t j = 0; j < 4; ++j)
      {
        if (m_interfaces[j].GetAddress(0) == srcIp)
          srcNode = 0;
        if (m_interfaces[j].GetAddress(0) == dstIp)
          dstNode = 0;
      }
    }

    if (srcNode != 0 || dstNode != 0)
    {
      m_outputFile << srcNode << "," << dstNode << "," << packet->GetSize() << ","
                   << txTime.GetSeconds() << "," << rxTime.GetSeconds() << "\n";
      m_packetCount++;
    }
  }
}

void
StarTopologyTcpDataset::SetupApplications()
{
  uint16_t port = 5000;

  for (uint32_t i = 1; i <= 4; ++i)
  {
    Address sinkLocalAddress(InetSocketAddress(m_interfaces[i - 1].GetAddress(1), port));
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApp = sinkHelper.Install(m_nodes.Get(i));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(10.0));

    OnOffHelper onoff("ns3::TcpSocketFactory", InetSocketAddress(m_interfaces[i - 1].GetAddress(1), port));
    onoff.SetConstantRate(DataRate("1Mbps"), 512);
    ApplicationContainer app = onoff.Install(m_nodes.Get(0));
    app.Start(Seconds(1.0));
    app.Stop(Seconds(9.0));
  }

  Config::Connect("/NodeList/*/ApplicationList/*/$ns3::PacketSink/RxWithAddresses",
                  MakeCallback(&StarTopologyTcpDataset::WritePacketDataToFile, this));
}

void
StarTopologyTcpDataset::SetupTracing()
{
  AsciiTraceHelper asciiTraceHelper;
  Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("star_topology_tcp.tr");
  for (uint32_t i = 0; i < 4; ++i)
  {
    p2p.EnableAsciiAll(stream);
  }
}

int main(int argc, char *argv[])
{
  LogComponentEnable("StarTopologyTcpDataset", LOG_LEVEL_INFO);

  StarTopologyTcpDataset topology;
  topology.SetupNodes();
  topology.SetupDevices();
  topology.SetupInternet();
  topology.SetupApplications();
  topology.SetupTracing();

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  NS_LOG_INFO("Simulation completed. Packets recorded: " << topology.m_packetCount);
  return 0;
}