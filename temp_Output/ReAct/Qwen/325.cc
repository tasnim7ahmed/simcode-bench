#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/tcp-header.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpRttSimulation");

class RttTrace : public Application
{
public:
  static TypeId GetTypeId(void)
  {
    TypeId tid = TypeId("RttTrace").SetParent<Application>();
    return tid;
  }

  RttTrace(Ptr<Socket> socket) : m_socket(socket) {}
  virtual ~RttTrace() {}

private:
  void StartApplication(void)
  {
    m_socket->TraceConnectWithoutContext("RTT", MakeCallback(&RttTrace::RttChanged, this));
  }

  void StopApplication(void) {}

  void RttChanged(Ptr<const RttEstimator> rtt, Time oldVal, Time newVal)
  {
    NS_LOG_INFO("RTT changed from " << oldVal.As(Time::MS) << " to " << newVal.As(Time::MS));
  }

  Ptr<Socket> m_socket;
};

int main(int argc, char *argv[])
{
  Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpNewReno::GetTypeId()));
  Config::SetDefault("ns3::TcpSocket::SndBufSize", UintegerValue(1 << 20));
  Config::SetDefault("ns3::TcpSocket::RcvBufSize", UintegerValue(1 << 20));

  NodeContainer nodes;
  nodes.Create(2);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", DataRateValue(DataRate("5Mbps")));
  p2p.SetChannelAttribute("Delay", TimeValue(MilliSeconds(10)));

  NetDeviceContainer devices = p2p.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  uint16_t sinkPort = 8080;
  Address sinkAddress(InetSocketAddress(interfaces.GetAddress(1), sinkPort));

  PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
  ApplicationContainer sinkApps = packetSinkHelper.Install(nodes.Get(1));
  sinkApps.Start(Seconds(1.0));
  sinkApps.Stop(Seconds(10.0));

  OnOffHelper client("ns3::TcpSocketFactory", sinkAddress);
  client.SetAttribute("DataRate", DataRateValue(DataRate("1Mbps")));
  client.SetAttribute("PacketSize", UintegerValue(1024));
  client.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  client.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));

  ApplicationContainer clientApps = client.Install(nodes.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  Ptr<Socket> clientSocket = DynamicCast<TcpSocket>(clientApps.Get(0)->GetObject<Socket>());
  if (clientSocket)
  {
    RttTrace *rttTrace = new RttTrace(clientSocket);
    Simulator::Schedule(Seconds(0), &RttTrace::StartApplication, rttTrace);
  }

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}