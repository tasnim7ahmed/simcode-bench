#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/packet.h"
#include <fstream>
#include <iomanip>
#include <string>
#include <map>
#include <vector>

using namespace ns3;

struct PacketRecord
{
  uint32_t packetUid;
  uint32_t srcNode;
  uint32_t dstNode;
  uint32_t size;
  double txTime;
  double rxTime;
};

std::map<uint32_t, std::vector<PacketRecord>> sentPackets; // packetUid -> list of records for possible multipath
std::ofstream csvFile;

void
TxCallback(Ptr<const Packet> packet, const Address &address, uint32_t srcNode, uint32_t dstNode)
{
  PacketRecord rec;
  rec.packetUid = packet->GetUid();
  rec.srcNode = srcNode;
  rec.dstNode = dstNode;
  rec.size = packet->GetSize();
  rec.txTime = Simulator::Now().GetSeconds();
  rec.rxTime = -1.0;
  sentPackets[rec.packetUid].push_back(rec);
}

void
RxCallback(Ptr<const Packet> packet, const Address &address, uint32_t dstNode)
{
  uint32_t uid = packet->GetUid();
  double rxTime = Simulator::Now().GetSeconds();
  if (sentPackets.find(uid) != sentPackets.end())
    {
      // Mark the first matching packet as delivered
      for (auto &rec : sentPackets[uid])
        {
          if (rec.rxTime < 0)
            {
              rec.rxTime = rxTime;
              // Output to CSV (src, dst, size, txTime, rxTime)
              csvFile << rec.srcNode << ",";
              csvFile << rec.dstNode << ",";
              csvFile << rec.size << ",";
              csvFile << std::fixed << std::setprecision(6) << rec.txTime << ",";
              csvFile << std::fixed << std::setprecision(6) << rec.rxTime << "\n";
              break;
            }
        }
    }
}

int
main(int argc, char *argv[])
{
  csvFile.open("star_tcp_dataset.csv", std::ios::out);
  csvFile << "src_node,dst_node,packet_size,tx_time,rx_time\n";

  NodeContainer centralNode;
  centralNode.Create(1);

  NodeContainer leafNodes;
  leafNodes.Create(4);

  // Create point-to-point links
  std::vector<NodeContainer> starLinks(4);
  for (uint32_t i = 0; i < 4; ++i)
    {
      starLinks[i] = NodeContainer(centralNode.Get(0), leafNodes.Get(i));
    }

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));

  InternetStackHelper stack;
  stack.Install(centralNode);
  stack.Install(leafNodes);

  std::vector<NetDeviceContainer> devices(4);
  for (uint32_t i = 0; i < 4; ++i)
    {
      devices[i] = p2p.Install(starLinks[i]);
    }

  // Assign IP addresses
  Ipv4AddressHelper address;
  std::vector<Ipv4InterfaceContainer> interfaces(4);
  for (uint32_t i = 0; i < 4; ++i)
    {
      std::ostringstream subnet;
      subnet << "10.1." << (i + 1) << ".0";
      address.SetBase(subnet.str().c_str(), "255.255.255.0");
      interfaces[i] = address.Assign(devices[i]);
    }

  // Install TCP server (PacketSink) on the four leaf nodes (ports 5000~5003)
  ApplicationContainer sinks;
  for (uint32_t i = 0; i < 4; ++i)
    {
      Address sinkAddress(InetSocketAddress(interfaces[i].GetAddress(1), 5000 + i));
      PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkAddress);
      ApplicationContainer sinkApp = sinkHelper.Install(leafNodes.Get(i));
      sinkApp.Start(Seconds(0.0));
      sinkApp.Stop(Seconds(12.0));
      sinks.Add(sinkApp);
    }

  // Install BulkSendApplication on central node to each leaf node
  for (uint32_t i = 0; i < 4; ++i)
    {
      BulkSendHelper bulkHelper("ns3::TcpSocketFactory",
                                InetSocketAddress(interfaces[i].GetAddress(1), 5000 + i));
      bulkHelper.SetAttribute("MaxBytes", UintegerValue(0));
      bulkHelper.SetAttribute("SendSize", UintegerValue(1024));
      ApplicationContainer app = bulkHelper.Install(centralNode.Get(0));
      app.Start(Seconds(1.0 + i));
      app.Stop(Seconds(11.0));
    }

  // Set up tracing callbacks
  for (uint32_t i = 0; i < 4; ++i)
    {
      // Outgoing (central node, to leaf)
      Ptr<NetDevice> dev = devices[i].Get(0);
      dev->TraceConnectWithoutContext(
          "PhyTxEnd",
          MakeBoundCallback(&TxCallback, 0, i + 1)
      );
      // Incoming (on leaf, from central)
      Ptr<NetDevice> dev2 = devices[i].Get(1);
      dev2->TraceConnectWithoutContext(
          "PhyRxEnd",
          MakeBoundCallback(&RxCallback, i + 1)
      );
    }

  Simulator::Stop(Seconds(12.0));
  Simulator::Run();
  Simulator::Destroy();
  csvFile.close();
  return 0;
}