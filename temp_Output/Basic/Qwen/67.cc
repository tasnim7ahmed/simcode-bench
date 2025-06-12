#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("DumbbellSimulation");

class DumbbellHelper {
public:
  DumbbellHelper();
  void Setup(const std::string& queueDiscType);
  void InstallApplications(Time startTime, Time stopTime);
  void EnableTraces();

private:
  NodeContainer m_leftLeafs;
  NodeContainer m_rightLeafs;
  NodeContainer m_routers;
  NetDeviceContainer m_leftDevices;
  NetDeviceContainer m_rightDevices;
  PointToPointHelper m_p2p;
  InternetStackHelper m_internet;
  Ipv4AddressHelper m_ipv4;
  uint16_t m_port;
};

DumbbellHelper::DumbbellHelper()
  : m_port(9)
{
  m_leftLeafs.Create(7);
  m_rightLeafs.Create(1);
  m_routers.Create(2);

  m_p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
  m_p2p.SetChannelAttribute("Delay", StringValue("10ms"));

  m_internet.Install(m_leftLeafs);
  m_internet.Install(m_rightLeafs);
  m_internet.Install(m_routers);
}

void DumbbellHelper::Setup(const std::string& queueDiscType)
{
  TrafficControlHelper tch;
  tch.SetRootQueueDisc(queueDiscType);

  NetDeviceContainer routerDevices;

  for (uint32_t i = 0; i < m_leftLeafs.GetN(); ++i)
    {
      NetDeviceContainer leftLink = m_p2p.Install(NodeContainer(m_leftLeafs.Get(i), m_routers.Get(0)));
      m_leftDevices.Add(leftLink);
    }

  NetDeviceContainer centralLink = m_p2p.Install(NodeContainer(m_routers.Get(0), m_routers.Get(1)));
  routerDevices = centralLink;
  tch.Install(centralLink);

  for (uint32_t i = 0; i < m_rightLeafs.GetN(); ++i)
    {
      NetDeviceContainer rightLink = m_p2p.Install(NodeContainer(m_routers.Get(1), m_rightLeafs.Get(i)));
      m_rightDevices.Add(rightLink);
    }

  Ipv4InterfaceContainer leftInterfaces;
  Ipv4InterfaceContainer rightInterfaces;
  Ipv4InterfaceContainer routerLeftInterfaces;
  Ipv4InterfaceContainer routerCentralInterfaces;

  m_ipv4.SetBase("10.1.1.0", "255.255.255.0");
  for (uint32_t i = 0; i < m_leftLeafs.GetN(); ++i)
    {
      Ipv4InterfaceContainer ifc = m_ipv4.Assign(NetDeviceContainer(m_leftDevices.Get(2*i), m_leftDevices.Get(2*i+1)));
      leftInterfaces.Add(ifc);
    }
  m_ipv4.NewNetwork();
  routerLeftInterfaces = m_ipv4.Assign(centralLink);
  m_ipv4.NewNetwork();
  for (uint32_t i = 0; i < m_rightLeafs.GetN(); ++i)
    {
      Ipv4InterfaceContainer ifc = m_ipv4.Assign(NetDeviceContainer(m_rightDevices.Get(2*i), m_rightDevices.Get(2*i+1)));
      rightInterfaces.Add(ifc);
    }

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();
}

void DumbbellHelper::InstallApplications(Time startTime, Time stopTime)
{
  uint32_t packetSize = 1000;
  uint32_t maxPacketCount = 1000000;
  Time interPacketInterval = MilliSeconds(10);

  for (uint32_t i = 0; i < m_leftLeafs.GetN(); ++i)
    {
      Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), m_port));
      PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
      ApplicationContainer sinkApp = packetSinkHelper.Install(m_rightLeafs.Get(0));
      sinkApp.Start(Seconds(0.0));
      sinkApp.Stop(stopTime);

      OnOffHelper onoff("ns3::UdpSocketFactory", Address());
      onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
      onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
      onoff.SetAttribute("DataRate", DataRateValue(DataRate("1Mbps")));
      onoff.SetAttribute("PacketSize", UintegerValue(packetSize));

      AddressValue remoteAddress(InetSocketAddress(m_ipv4.GetAddress(2, 0), m_port));
      onoff.SetAttribute("Remote", remoteAddress);

      ApplicationContainer app = onoff.Install(m_leftLeafs.Get(i));
      app.Start(startTime);
      app.Stop(stopTime);

      m_port++;
    }
}

void
CwndChange(std::string path, Time oldVal, Time newVal)
{
  static ofstream cwndFile("tcp-cwnd-trace.txt");
  cwndFile << Simulator::Now().GetSeconds() << " " << newVal.GetDouble() << std::endl;
}

void
QueueSizeTrace(Ptr<OutputStreamWrapper> stream, Ptr<const QueueDiscItem> item)
{
  *stream->GetStream() << Simulator::Now().GetSeconds() << " " << item->GetSize() << std::endl;
}

void DumbbellHelper::EnableTraces()
{
  Config::ConnectWithoutContext("/NodeList/*/ApplicationList/*/$ns3::TcpL4Protocol/SocketList/0/CongestionWindow", MakeCallback(&CwndChange));

  for (uint32_t i = 0; i < m_leftLeafs.GetN(); ++i)
    {
      std::ostringstream oss;
      oss << "queue-disc-" << i << ".txt";
      AsciiTraceHelper asciiTraceHelper;
      Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream(oss.str());
      Config::Connect("/NodeList/1/$ns3::TrafficControlLayer/RootQueueDisc/Enqueue", MakeBoundCallback(&QueueSizeTrace, stream));
    }
}

void experiment(const std::string& queueDiscType)
{
  DumbbellHelper dumbbell;
  dumbbell.Setup(queueDiscType);
  dumbbell.InstallApplications(Seconds(1.0), Seconds(10.0));
  dumbbell.EnableTraces();
  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();
}

int main(int argc, char* argv[])
{
  std::string queueDiscType1 = "ns3::CoDelQueueDisc";
  std::string queueDiscType2 = "ns3::COBALTQueueDisc";

  experiment(queueDiscType1);
  experiment(queueDiscType2);

  return 0;
}