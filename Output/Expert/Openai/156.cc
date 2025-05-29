#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include <fstream>
#include <iomanip>

using namespace ns3;

class DatasetCollector
{
public:
  DatasetCollector ();
  void LogTx (Ptr<const Packet> pkt, const Address &src);
  void LogRx (Ptr<const Packet> pkt, const Address &addr);
  void SaveCsv (const std::string &filename);

  struct Record
  {
    uint32_t src;
    uint32_t dst;
    uint32_t size;
    double txTime;
    double rxTime;
    uint64_t uid;
  };
private:
  std::vector<Record> m_records;
  std::map<uint64_t, std::tuple<double, uint32_t, uint32_t, uint32_t>> m_txMap;
};

DatasetCollector::DatasetCollector () {}

void
DatasetCollector::LogTx (Ptr<const Packet> pkt, const Address &src)
{
  uint64_t uid = pkt->GetUid ();
  uint32_t srcNode = InetSocketAddress::ConvertFrom (src).GetIpv4 ().IsAny () ? 0 : 0;
  // Source node is central node: 0
  // Store: txTime, src, to be extracted at Rx
  m_txMap[uid] = std::make_tuple (Simulator::Now ().GetSeconds (), 0, 0, pkt->GetSize ());
}

void
DatasetCollector::LogRx (Ptr<const Packet> pkt, const Address &address)
{
  uint64_t uid = pkt->GetUid ();
  uint32_t dstNode = 0;
  Ipv4Address ip = InetSocketAddress::ConvertFrom (address).GetIpv4 ();
  if      (ip == "10.1.1.2") dstNode = 1;
  else if (ip == "10.1.2.2") dstNode = 2;
  else if (ip == "10.1.3.2") dstNode = 3;
  else if (ip == "10.1.4.2") dstNode = 4;
  else dstNode = 0;

  auto it = m_txMap.find (uid);
  if (it != m_txMap.end ())
    {
      double txTime = std::get<0> (it->second);
      uint32_t srcNode = 0;
      uint32_t size = pkt->GetSize ();
      m_records.push_back ({srcNode, dstNode, size, txTime, Simulator::Now ().GetSeconds (), uid});
      m_txMap.erase (it);
    }
}

void
DatasetCollector::SaveCsv (const std::string &filename)
{
  std::ofstream out (filename, std::ios::trunc);
  out << "Source,Destination,PacketSize,TxTime,RxTime\n";
  for (const auto &r : m_records)
    {
      out << r.src << "," << r.dst << "," << r.size << ","
          << std::fixed << std::setprecision (9) << r.txTime << ","
          << std::fixed << std::setprecision (9) << r.rxTime << "\n";
    }
  out.close ();
}

DatasetCollector collector;

void
TxCallback (Ptr<const Packet> pkt, const Address &src)
{
  collector.LogTx (pkt, src);
}
void
RxCallback (Ptr<const Packet> pkt, const Address &address)
{
  collector.LogRx (pkt, address);
}

int
main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  NodeContainer central;
  central.Create (1);
  NodeContainer peripherals;
  peripherals.Create (4);

  NodeContainer allNodes;
  allNodes.Add (central);
  allNodes.Add (peripherals);

  std::vector<NetDeviceContainer> deviceVec (4);
  std::vector<NodeContainer> pairNodes (4);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  InternetStackHelper stack;
  stack.Install (allNodes);

  Ipv4AddressHelper address;
  std::vector<Ipv4InterfaceContainer> interfaces (4);

  for (uint32_t i = 0; i < 4; ++i)
    {
      NodeContainer pair (central.Get (0), peripherals.Get (i));
      deviceVec[i] = p2p.Install (pair);
      std::ostringstream subnet;
      subnet << "10.1." << (i+1) << ".0";
      address.SetBase (subnet.str ().c_str (), "255.255.255.0");
      interfaces[i] = address.Assign (deviceVec[i]);
    }

  uint16_t port = 50000;
  ApplicationContainer sinkApps, sourceApps;

  for (uint32_t i = 0; i < 4; ++i)
    {
      Address sinkAddress (InetSocketAddress (interfaces[i].GetAddress (1), port));
      PacketSinkHelper sink ("ns3::TcpSocketFactory", sinkAddress);
      ApplicationContainer app = sink.Install (peripherals.Get (i));
      app.Start (Seconds (0.0));
      app.Stop (Seconds (10.0));
      sinkApps.Add (app);

      // BulkSend settings
      BulkSendHelper source ("ns3::TcpSocketFactory", sinkAddress);
      source.SetAttribute ("MaxBytes", UintegerValue (0)); // Unlimited
      source.SetAttribute ("SendSize", UintegerValue (1024));

      ApplicationContainer srcApp = source.Install (central.Get (0));
      srcApp.Start (Seconds (1.0 + i*0.1)); // Staggered start
      srcApp.Stop (Seconds (9.0));
      sourceApps.Add (srcApp);
    }

  // Trace connections for dataset
  for (uint32_t i = 0; i < 4; ++i)
    {
      Ptr<Node> sender = central.Get (0);
      Ptr<Node> receiver = peripherals.Get (i);

      std::ostringstream pathTx;
      pathTx << "/NodeList/" << sender->GetId () << "/ApplicationList/0/$ns3::BulkSendApplication/Tx";
      Config::ConnectWithoutContext (pathTx.str (), MakeCallback (&TxCallback));

      std::ostringstream pathRx;
      pathRx << "/NodeList/" << receiver->GetId () << "/ApplicationList/0/$ns3::PacketSink/Rx";
      Config::ConnectWithoutContext (pathRx.str (), MakeCallback (&RxCallback));
    }

  Simulator::Stop (Seconds (10.1));
  Simulator::Run ();

  collector.SaveCsv ("star_dataset.csv");

  Simulator::Destroy ();
  return 0;
}