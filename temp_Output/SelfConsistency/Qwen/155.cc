#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/tcp-westwood.h"
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("StarTopologyTcpDataset");

class StarTopologyExperiment
{
public:
  void Run();
  void WriteHeader(std::ofstream &file);
  void TraceTx(Ptr<const Packet> packet, const TcpHeader &header, Ptr<const TcpSocketBase> socket);
  void TraceRx(Ptr<const Packet> packet, const TcpHeader &header, Ptr<const TcpSocketBase> socket);

private:
  std::map<uint32_t, uint32_t> m_flowIdToSrc;
  std::map<uint32_t, uint32_t> m_flowIdToDst;
  uint32_t m_nextFlowId = 0;
};

void StarTopologyExperiment::WriteHeader(std::ofstream &file)
{
  file << "Source,Destination,PacketSize,TransmissionTime,ReceptionTime\n";
}

void StarTopologyExperiment::TraceTx(Ptr<const Packet> packet, const TcpHeader &header, Ptr<const TcpSocketBase> socket)
{
  Time txTime = Simulator::Now();
  uint32_t flowId = header.GetDestinationPort(); // Simplified mapping for flow ID
  uint32_t src = m_flowIdToSrc[flowId];
  uint32_t dst = m_flowIdToDst[flowId];

  NS_LOG_INFO("TX from " << src << " to " << dst << ", size=" << packet->GetSize() << " at " << txTime.As(Time::S));
}

void StarTopologyExperiment::TraceRx(Ptr<const Packet> packet, const TcpHeader &header, Ptr<const TcpSocketBase> socket)
{
  Time rxTime = Simulator::Now();
  uint32_t flowId = header.GetSourcePort(); // Reverse port for incoming data
  uint32_t src = m_flowIdToSrc[flowId];
  uint32_t dst = m_flowIdToDst[flowId];

  static std::ofstream outFile("tcp_dataset.csv");
  static bool headerWritten = false;
  if (!headerWritten)
    {
      WriteHeader(outFile);
      headerWritten = true;
    }

  outFile << src << "," << dst << "," << packet->GetSize() << "," << (Simulator::Now() - Seconds(1.0)).GetSeconds() << "," << rxTime.GetSeconds() << "\n";

  NS_LOG_INFO("RX at " << dst << " from " << src << ", size=" << packet->GetSize() << " at " << rxTime.As(Time::S));
}

void StarTopologyExperiment::Run()
{
  Time::SetResolution(Time::NS);
  LogComponentEnable("StarTopologyTcpDataset", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create(5); // node 0 is central hub

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("1ms"));

  NetDeviceContainer devices[5];
  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  Ipv4InterfaceContainer interfaces[5];

  for (uint32_t i = 1; i <= 4; ++i)
    {
      devices[i - 1] = p2p.Install(NodeContainer(nodes.Get(0), nodes.Get(i)));
      address.SetBase("10.1." + std::to_string(i) + ".0", "255.255.255.0");
      interfaces[i - 1] = address.Assign(devices[i - 1]);
    }

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  uint16_t sinkPort = 8080;

  for (uint32_t i = 1; i <= 4; ++i)
    {
      Address sinkAddress(InetSocketAddress(interfaces[i - 1].GetAddress(1), sinkPort));

      PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
      ApplicationContainer sinkApp = packetSinkHelper.Install(nodes.Get(i));
      sinkApp.Start(Seconds(0.0));
      sinkApp.Stop(Seconds(10.0));

      OnOffHelper onoff("ns3::TcpSocketFactory", sinkAddress);
      onoff.SetConstantRate(DataRate("1Mbps"), 512);
      onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
      onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));

      ApplicationContainer app = onoff.Install(nodes.Get(0));
      app.Start(Seconds(1.0));
      app.Stop(Seconds(9.0));

      uint32_t flowId = sinkPort++;
      m_flowIdToSrc[flowId] = 0;
      m_flowIdToDst[flowId] = i;
    }

  Config::Connect("/NodeList/*/ApplicationList/*/$ns3::PacketSink/Rx",
                  MakeCallback(&StarTopologyExperiment::TraceRx, this));
  Config::ConnectWithoutContext("/NodeList/*/DeviceList/*/$ns3::PointToPointNetDevice/MacTx", 
                                MakeCallback(&StarTopologyExperiment::TraceTx, this));

  Simulator::Run();
  Simulator::Destroy();
}

int main(int argc, char *argv[])
{
  StarTopologyExperiment experiment;
  experiment.Run();
  return 0;
}