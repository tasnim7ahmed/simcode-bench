#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include <fstream>
#include <vector>
#include <string>
#include <iomanip>

using namespace ns3;

struct PacketRecord {
  uint32_t sourceNode;
  uint32_t destNode;
  uint32_t packetSize;
  double txTime;
  double rxTime;
};

std::vector<PacketRecord> g_records;
std::map<uint64_t, std::pair<double, uint32_t>> g_txTimeMap; // key: packet uid, value: <txTime, sourceNode>
std::string g_csvFilename = "star_dataset.csv";

void
TxTrace(Ptr<const Packet> packet, const Address &from, const Address &to, uint32_t id)
{
  double now = Simulator::Now().GetSeconds();
  uint64_t uid = packet->GetUid();
  g_txTimeMap[uid] = std::make_pair(now, id);
}

void
RxTrace(Ptr<const Packet> packet, const Address &addr, uint32_t destNodeId)
{
  uint64_t uid = packet->GetUid();
  auto it = g_txTimeMap.find(uid);
  if (it != g_txTimeMap.end())
    {
      PacketRecord rec;
      rec.sourceNode = it->second.second;
      rec.destNode = destNodeId;
      rec.packetSize = packet->GetSize();
      rec.txTime = it->second.first;
      rec.rxTime = Simulator::Now().GetSeconds();
      g_records.push_back(rec);
      g_txTimeMap.erase(it); // free a little memory
    }
}

void
WriteRecordsToCsv()
{
  std::ofstream file(g_csvFilename);
  file << "src,dst,packet_size,tx_time,rx_time\n";
  for (const auto &rec : g_records)
    file << rec.sourceNode << "," << rec.destNode << "," << rec.packetSize
         << "," << std::fixed << std::setprecision(9) << rec.txTime << "," << rec.rxTime << "\n";
  file.close();
}

int
main(int argc, char *argv[])
{
  uint32_t numSpokes = 4;
  double simTime = 5.0; // seconds
  uint32_t packetSize = 1024;
  std::string dataRate = "10Mbps";
  std::string delay = "2ms";

  CommandLine cmd;
  cmd.AddValue("output", "CSV output filename", g_csvFilename);
  cmd.Parse(argc, argv);

  NodeContainer center;
  center.Create(1);
  NodeContainer spokes;
  spokes.Create(numSpokes);

  // Each pair: central <-> spoke
  std::vector<NodeContainer> starLinks;
  for (uint32_t i = 0; i < numSpokes; ++i)
    {
      NodeContainer pair;
      pair.Add(center.Get(0));
      pair.Add(spokes.Get(i));
      starLinks.push_back(pair);
    }

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue(dataRate));
  p2p.SetChannelAttribute("Delay", StringValue(delay));

  std::vector<NetDeviceContainer> devices;
  for (auto &link : starLinks)
    devices.push_back(p2p.Install(link));

  InternetStackHelper internet;
  internet.Install(center);
  internet.Install(spokes);

  Ipv4AddressHelper ipv4;
  std::vector<Ipv4InterfaceContainer> interfaces;
  for (uint32_t i = 0; i < numSpokes; ++i)
    {
      std::ostringstream subnet;
      subnet << "10.1." << i + 1 << ".0";
      ipv4.SetBase(subnet.str().c_str(), "255.255.255.0");
      interfaces.push_back(ipv4.Assign(devices[i]));
    }

  // Set up TCP server (sink) Apps on spokes and TCP clients on center node
  uint16_t portBase = 5000;
  ApplicationContainer serverApps, clientApps;
  for (uint32_t i = 0; i < numSpokes; ++i)
    {
      Address bindAddr(InetSocketAddress(Ipv4Address::GetAny(), portBase + i));
      Ptr<PacketSink> sink = CreateObject<PacketSink>();
      sink->SetAttribute("Local", AddressValue(bindAddr));
      center.Get(0)->AggregateObject(sink);

      PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), portBase + i));
      ApplicationContainer app = sinkHelper.Install(spokes.Get(i));
      app.Start(Seconds(0.0));
      app.Stop(Seconds(simTime));
      serverApps.Add(app);

      // OnOffApplication as TCP Client
      OnOffHelper clientHelper("ns3::TcpSocketFactory",
                               InetSocketAddress(interfaces[i].GetAddress(1), portBase + i));
      clientHelper.SetAttribute("DataRate", DataRateValue(DataRate("5Mbps")));
      clientHelper.SetAttribute("PacketSize", UintegerValue(packetSize));
      clientHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
      clientHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
      ApplicationContainer clientApp = clientHelper.Install(center.Get(0));
      clientApp.Start(Seconds(0.5 + i * 0.2)); // staggered start
      clientApp.Stop(Seconds(simTime));
      clientApps.Add(clientApp);
    }

  // Connect Tx/Rx Traces for data set collection
  for (uint32_t i = 0; i < numSpokes; ++i)
    {
      // Tx on central node device (index 0 of pair)
      Ptr<NetDevice> dev = devices[i].Get(0);
      dev->TraceConnectWithoutContext("PhyTxEnd",
        MakeBoundCallback(&TxTrace, 0 /*center node*/));
      // Rx on spoke node device (index 1 of pair)
      Ptr<NetDevice> devSpoke = devices[i].Get(1);
      devSpoke->TraceConnectWithoutContext("PhyRxEnd",
        MakeBoundCallback(&RxTrace, 1 + i));
    }

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  Simulator::Stop(Seconds(simTime));
  Simulator::Run();

  WriteRecordsToCsv();

  Simulator::Destroy();
  return 0;
}