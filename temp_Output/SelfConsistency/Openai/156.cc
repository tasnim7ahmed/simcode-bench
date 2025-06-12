#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include <fstream>
#include <iomanip>

using namespace ns3;

struct PacketLogEntry
{
  uint32_t srcNode;
  uint32_t dstNode;
  uint32_t packetSize;
  double   txTime;
  double   rxTime;
};

std::vector<PacketLogEntry> packetLog;

// Mapping of Tx UID to log entry index
std::map<uint64_t, size_t> uidToLogIndex;

// Helper to get node id from IPv4/IP address
uint32_t
GetNodeIdFromAddress (Ptr<Node> node, Ipv4Address address)
{
  Ptr<Ipv4> ipv4 = node->GetObject<Ipv4> ();
  for (uint32_t i = 0; i < ipv4->GetNInterfaces (); ++i)
    {
      for (uint32_t j = 0; j < ipv4->GetNAddresses (i); ++j)
        {
          if (ipv4->GetAddress (i, j).GetLocal () == address)
            {
              return node->GetId ();
            }
        }
    }
  return 0; // fallback, should not happen
}

void
TxTrace (Ptr<const Packet> pkt, const Address &address, uint32_t srcNode, uint32_t dstNode)
{
  PacketLogEntry entry;
  entry.srcNode = srcNode;
  entry.dstNode = dstNode;
  entry.packetSize = pkt->GetSize ();
  entry.txTime = Simulator::Now ().GetSeconds ();
  entry.rxTime = -1.0; // To be filled
  // We use the packet's UID as unique identifier
  uidToLogIndex[pkt->GetUid ()] = packetLog.size ();
  packetLog.push_back (entry);
}

void
RxTrace (Ptr<const Packet> pkt, const Address &address, uint32_t rxNode)
{
  auto it = uidToLogIndex.find (pkt->GetUid ());
  if (it != uidToLogIndex.end ())
    {
      size_t idx = it->second;
      packetLog[idx].rxTime = Simulator::Now ().GetSeconds ();
    }
}

int
main (int argc, char *argv[])
{
  // Logging and simulation parameters
  uint32_t nPeripheralNodes = 4;
  uint32_t packetSize = 1024;
  double simTime = 5.0;
  std::string dataRate = "5Mbps";
  std::string delay = "2ms";
  std::string csvFileName = "star_topology_dataset.csv";

  // 0. Create 5 nodes
  NodeContainer nodes;
  nodes.Create (nPeripheralNodes + 1); // node 0 is central

  // 1. Create point-to-point links
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue (dataRate));
  p2p.SetChannelAttribute ("Delay", StringValue (delay));

  std::vector<NetDeviceContainer> deviceContainers (nPeripheralNodes);
  for (uint32_t i = 0; i < nPeripheralNodes; ++i)
    {
      deviceContainers[i] = p2p.Install (NodeContainer (nodes.Get (0), nodes.Get (i + 1)));
    }

  // 2. Install Internet stack
  InternetStackHelper stack;
  stack.Install (nodes);

  // 3. Assign IP addresses
  Ipv4AddressHelper address;
  std::vector<Ipv4InterfaceContainer> interfaces (nPeripheralNodes);
  for (uint32_t i = 0; i < nPeripheralNodes; ++i)
    {
      std::ostringstream subnet;
      subnet << "10.1." << i + 1 << ".0";
      address.SetBase (Ipv4Address (subnet.str ().c_str ()), "255.255.255.0");
      interfaces[i] = address.Assign (deviceContainers[i]);
    }

  // 4. Setup TCP applications (central node sends to each peripheral node)
  uint16_t basePort = 50000;
  ApplicationContainer clientApps;
  ApplicationContainer serverApps;

  for (uint32_t i = 0; i < nPeripheralNodes; ++i)
    {
      // Install PacketSink (TCP server) on peripheral nodes
      Address sinkAddress (InetSocketAddress (interfaces[i].GetAddress (1), basePort + i));
      PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", sinkAddress);
      serverApps.Add (sinkHelper.Install (nodes.Get (i + 1)));

      // Create BulkSendApplication (TCP client) on central node to send to peripheral
      BulkSendHelper clientHelper ("ns3::TcpSocketFactory", sinkAddress);
      clientHelper.SetAttribute ("MaxBytes", UintegerValue (0)); // unlimited
      clientHelper.SetAttribute ("SendSize", UintegerValue (packetSize));
      clientApps.Add (clientHelper.Install (nodes.Get (0)));
    }

  // Set application start/stop times
  serverApps.Start (Seconds (0.0));
  serverApps.Stop (Seconds (simTime));
  clientApps.Start (Seconds (1.0));
  clientApps.Stop (Seconds (simTime));

  // 5. Trace packet transmission and reception
  // Attach transmit traces to the socket layer on node 0 outgoing interfaces
  for (uint32_t i = 0; i < nPeripheralNodes; ++i)
    {
      Ptr<NetDevice> dev = deviceContainers[i].Get (0); // node 0's NetDevice (sender)
      Ptr<Node> srcNode = nodes.Get (0);
      Ptr<Node> dstNode = nodes.Get (i + 1);

      dev->TraceConnectWithoutContext (
          "PhyTxEnd",
          MakeBoundCallback (
              [](Ptr<const Packet> pkt, const Address &addr, uint32_t src, uint32_t dst) {
                TxTrace (pkt, addr, src, dst);
              },
              srcNode->GetId (), dstNode->GetId ()));
    }

  // Attach reception traces on each PacketSink node's NetDevice
  for (uint32_t i = 0; i < nPeripheralNodes; ++i)
    {
      Ptr<NetDevice> dev = deviceContainers[i].Get (1); // peripheral's NetDevice (receiver)
      Ptr<Node> dstNode = nodes.Get (i + 1);

      dev->TraceConnectWithoutContext (
          "PhyRxEnd",
          MakeBoundCallback (
              [](Ptr<const Packet> pkt, const Address &addr, uint32_t dst) {
                RxTrace (pkt, addr, dst);
              },
              dstNode->GetId ()));
    }

  Simulator::Stop (Seconds (simTime + 1.0));
  Simulator::Run ();
  Simulator::Destroy ();

  // 6. Save dataset to CSV file
  std::ofstream csv (csvFileName, std::ios_base::out);
  csv << "src_node,dst_node,packet_size,tx_time,rx_time\n";
  for (const auto &entry : packetLog)
    {
      csv << entry.srcNode << ","
          << entry.dstNode << ","
          << entry.packetSize << ","
          << std::fixed << std::setprecision (6) << entry.txTime << ","
          << std::fixed << std::setprecision (6) << (entry.rxTime >= 0 ? entry.rxTime : 0.0) << "\n";
    }
  csv.close ();
  std::cout << "Dataset saved: " << csvFileName << std::endl;

  return 0;
}