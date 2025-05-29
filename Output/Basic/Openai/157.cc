#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include <fstream>
#include <string>
#include <sstream>

using namespace ns3;

struct PacketRecord {
  uint32_t src;
  uint32_t dst;
  uint32_t size;
  double txTime;
  double rxTime;
};

std::vector<PacketRecord> g_records;

// Mapping packet UIDs to send times and src/dst
std::map<uint64_t, std::tuple<uint32_t,uint32_t,uint32_t,double>> g_packetInfo;

void
TxTrace(Ptr<const Packet> packet, const Address &address, uint32_t srcNode, uint32_t dstNode)
{
  double now = Simulator::Now().GetSeconds();
  uint64_t uid = packet->GetUid();
  uint32_t size = packet->GetSize();
  g_packetInfo[uid] = std::make_tuple(srcNode, dstNode, size, now);
}

void
RxTrace(Ptr<const Packet> packet, const Address &address)
{
  double now = Simulator::Now().GetSeconds();
  uint64_t uid = packet->GetUid();
  auto it = g_packetInfo.find(uid);
  if (it != g_packetInfo.end())
    {
      uint32_t src = std::get<0>(it->second);
      uint32_t dst = std::get<1>(it->second);
      uint32_t size = std::get<2>(it->second);
      double txTime = std::get<3>(it->second);
      g_records.push_back({src, dst, size, txTime, now});
      g_packetInfo.erase(it);
    }
}

int
main(int argc, char *argv[])
{
  uint32_t numNodes = 4;
  uint32_t packetSize = 512;
  double interval = 0.05;
  uint32_t nPackets = 20;

  NodeContainer nodes;
  nodes.Create(numNodes);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer devices[numNodes];
  for (uint32_t i = 0; i < numNodes; ++i)
    {
      devices[i] = p2p.Install(nodes.Get(i), nodes.Get((i+1)%numNodes));
    }

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  Ipv4InterfaceContainer interfaces[numNodes];

  for (uint32_t i = 0; i < numNodes; ++i)
    {
      std::ostringstream subnet;
      subnet << "10.1." << i+1 << ".0";
      address.SetBase(subnet.str().c_str(), "255.255.255.0");
      interfaces[i] = address.Assign(devices[i]);
    }

  uint16_t port = 9000;
  ApplicationContainer apps;
  std::vector<Ptr<PacketSink>> sinks(numNodes);

  for (uint32_t i = 0; i < numNodes; ++i)
    {
      // Install receiver on node (i+1)%numNodes
      Address sinkAddr(InetSocketAddress (interfaces[i].GetAddress(1), port));
      PacketSinkHelper sinkHelper ("ns3::UdpSocketFactory", sinkAddr);
      apps = sinkHelper.Install(nodes.Get((i+1)%numNodes));
      sinks[(i+1)%numNodes] = DynamicCast<PacketSink>(apps.Get(0));
    }

  for (uint32_t i = 0; i < numNodes; ++i)
    {
      uint32_t dst = (i+1)%numNodes;
      UdpClientHelper client (interfaces[i].GetAddress(1), port);
      client.SetAttribute ("MaxPackets", UintegerValue (nPackets));
      client.SetAttribute ("Interval", TimeValue (Seconds (interval)));
      client.SetAttribute ("PacketSize", UintegerValue (packetSize));
      apps = client.Install(nodes.Get(i));
    }

  // Connect traces for all nodes/apps
  for (uint32_t i = 0; i < numNodes; ++i)
    {
      Ptr<Node> txNode = nodes.Get(i);
      Ptr<Node> rxNode = nodes.Get((i+1)%numNodes);

      // Connect Tx trace to UDP socket
      for (uint32_t j = 0; j < txNode->GetNApplications(); ++j)
        {
          Ptr<Application> app = txNode->GetApplication(j);
          Ptr<UdpClient> udpClient = DynamicCast<UdpClient>(app);
          if (udpClient)
            {
              udpClient->TraceConnectWithoutContext(
                "Tx", MakeBoundCallback(&TxTrace, i, (i+1)%numNodes));
            }
        }
      // Connect Rx trace on sink
      if (sinks[(i+1)%numNodes])
        {
          sinks[(i+1)%numNodes]->TraceConnectWithoutContext(
            "Rx", MakeCallback(&RxTrace));
        }
    }

  Simulator::Stop(Seconds(nPackets * interval + 2));
  Simulator::Run();

  std::ofstream out("ring_dataset.csv");
  out << "src_node,dst_node,packet_size,tx_time,rx_time\n";
  for (const auto &rec : g_records)
    {
      out << rec.src << "," << rec.dst << "," << rec.size << "," << rec.txTime
          << "," << rec.rxTime << "\n";
    }
  out.close();

  Simulator::Destroy();
  return 0;
}