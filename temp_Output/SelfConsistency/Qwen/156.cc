#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/tcp-westwood.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("StarTopologyTcpDataset");

class StarTopologyTcpDataset : public Object
{
public:
  static TypeId GetTypeId(void);
  void ReceivePacket(Ptr<Socket> socket);
};

TypeId
StarTopologyTcpDataset::GetTypeId(void)
{
  static TypeId tid = TypeId("StarTopologyTcpDataset")
                          .SetParent<Object>()
                          .AddConstructor<StarTopologyTcpDataset>();
  return tid;
}

void
StarTopologyTcpDataset::ReceivePacket(Ptr<Socket> socket)
{
  Address from;
  while (Ptr<Packet> packet = socket->RecvFrom(from))
  {
    Ipv4Address srcAddr = InetSocketAddress::ConvertFrom(from).GetIpv4();
    uint16_t srcPort = InetSocketAddress::ConvertFrom(from).GetPort();

    // Find source node ID
    NodeContainer nodes = NodeContainer::GetGlobal();
    uint32_t srcNodeId = 0;
    for (uint32_t i = 0; i < nodes.GetN(); ++i)
    {
      Ptr<Ipv4> ipv4 = nodes.Get(i)->GetObject<Ipv4>();
      for (uint32_t j = 0; j < ipv4->GetNInterfaces(); ++j)
      {
        for (uint32_t k = 0; k < ipv4->GetNAddresses(j); ++k)
        {
          if (ipv4->GetAddress(j, k).GetLocal() == srcAddr)
          {
            srcNodeId = i;
            break;
          }
        }
      }
    }

    // Destination is always the central node (node 0)
    uint32_t destNodeId = 0;
    uint32_t packetSize = packet->GetSize();
    double txTime = Simulator::Now().GetSeconds();

    // Output format: source_node,destination_node,packet_size,transmission_time,reception_time
    std::ofstream datasetFile("star_topology_dataset.csv", std::ios::app);
    datasetFile << srcNodeId << "," 
                << destNodeId << ","
                << packetSize << ","
                << txTime << ","
                << txTime << "\n";
    datasetFile.close();
  }
}

int main(int argc, char *argv[])
{
  CommandLine cmd(__FILE__);
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(5); // node 0 is central

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("1Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer centralDevice;
  Ipv4InterfaceContainer centralInterface;

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");

  // Connect each peripheral node to the central node
  for (uint32_t i = 1; i <= 4; ++i)
  {
    NodeContainer pair(nodes.Get(0), nodes.Get(i));
    NetDeviceContainer devices = p2p.Install(pair);
    Ipv4InterfaceContainer interfaces = address.Assign(devices);
    address.NewNetwork();
  }

  // Install TCP receiver on central node (node 0)
  uint16_t sinkPort = 8080;
  PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
  ApplicationContainer sinkApp = packetSinkHelper.Install(nodes.Get(0));
  sinkApp.Start(Seconds(0.0));
  sinkApp.Stop(Seconds(10.0));

  // Install TCP senders on peripheral nodes (nodes 1-4)
  for (uint32_t i = 1; i <= 4; ++i)
  {
    OnOffHelper onoff("ns3::TcpSocketFactory", InetSocketAddress(nodes.Get(0)->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal(), sinkPort));
    onoff.SetConstantRate(DataRate("100kbps"));
    onoff.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer app = onoff.Install(nodes.Get(i));
    app.Start(Seconds(1.0));
    app.Stop(Seconds(9.0));
  }

  // Setup tracing for capturing packets at the central node
  Ptr<StarTopologyTcpDataset> traceHelper = CreateObject<StarTopologyTcpDataset>();
  Config::Connect("/NodeList/0/ApplicationList/*/$ns3::PacketSink/Rx", MakeCallback(&StarTopologyTcpDataset::ReceivePacket, traceHelper));

  // Ensure CSV file starts with headers
  std::ofstream datasetFile("star_topology_dataset.csv");
  datasetFile << "source_node,destination_node,packet_size,transmission_time,reception_time\n";
  datasetFile.close();

  Simulator::Run();
  Simulator::Destroy();

  return 0;
}