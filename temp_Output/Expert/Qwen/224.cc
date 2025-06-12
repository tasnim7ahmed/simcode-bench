#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/aodv-module.h"
#include "ns3/olsr-module.h"
#include "ns3/dsdv-module.h"
#include "ns3/dsr-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/flow-monitor-helper.h"
#include <vector>
#include <algorithm>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ManetSimulation");

class UdpNeighborSender : public Application
{
public:
  static TypeId GetTypeId();
  UdpNeighborSender();
  virtual ~UdpNeighborNeighbor();

protected:
  void StartApplication() override;
  void StopApplication() override;

private:
  void SendPacket();
  void ScheduleNextTx();

  Ptr<Socket> m_socket;
  Address m_destAddr;
  uint32_t m_packetSize;
  DataRate m_dataRate;
  EventId m_sendEvent;
  bool m_isRunning;
  std::vector<Ipv4Address> m_neighbors;
};

TypeId
UdpNeighborSender::GetTypeId()
{
  static TypeId tid = TypeId("UdpNeighborSender")
                          .SetParent<Application>()
                          .AddConstructor<UdpNeighborSender>()
                          .AddAttribute("RemoteAddress",
                                        "The destination address for outgoing packets.",
                                        AddressValue(),
                                        MakeAddressAccessor(&UdpNeighborSender::m_destAddr),
                                        MakeAddressChecker())
                          .AddAttribute("PacketSize",
                                        "Size of the packet to send (bytes).",
                                        UintegerValue(1024),
                                        MakeUintegerAccessor(&UdpNeighborSender::m_packetSize),
                                        MakeUintegerChecker<uint32_t>(1))
                          .AddAttribute("DataRate",
                                        "The data rate for sending packets.",
                                        DataRateValue(DataRate("100kbps")),
                                        MakeDataRateAccessor(&UdpNeighborSender::m_dataRate),
                                        MakeDataRateChecker());
  return tid;
}

UdpNeighborSender::UdpNeighborSender()
    : m_socket(nullptr),
      m_packetSize(1024),
      m_dataRate("100kbps"),
      m_isRunning(false)
{
}

UdpNeighborSender::~UdpNeighborSender()
{
  m_sendEvent.Cancel();
}

void
UdpNeighborSender::StartApplication()
{
  m_isRunning = true;
  m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());

  InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), 9);
  m_socket->Bind(local);

  ScheduleNextTx();
}

void
UdpNeighborSender::StopApplication()
{
  m_isRunning = false;
  if (m_socket)
    {
      m_socket->Close();
      m_socket = nullptr;
    }

  Simulator::Cancel(m_sendEvent);
}

void
UdpNeighborSender::ScheduleNextTx()
{
  if (!m_isRunning)
    return;

  Time interval = Seconds(1.0);
  m_sendEvent = Simulator::Schedule(interval, &UdpNeighborSender::SendPacket, this);
}

void
UdpNeighborSender::SendPacket()
{
  if (!m_isRunning || m_neighbors.empty())
    {
      ScheduleNextTx();
      return;
    }

  // Randomly choose a neighbor
  uint32_t index = rand() % m_neighbors.size();
  Ipv4Address destIp = m_neighbors[index];

  m_destAddr = InetSocketAddress(destIp, 9);

  Ptr<Packet> packet = Create<Packet>(m_packetSize);
  m_socket->SendTo(packet, 0, m_destAddr);

  NS_LOG_INFO(Simulator::Now().As(Time::S) << " Node " << GetNode()->GetId()
                                          << " sent packet to " << destIp);

  ScheduleNextTx();
}

void
UpdateNeighbors(Ptr<Node> node, Ipv4InterfaceContainer interfaces, std::vector<std::vector<Ipv4Address>>& allAddresses, uint32_t nodeId)
{
  // Simple neighbor detection based on proximity in container
  std::vector<Ipv4Address> neighbors;

  for (uint32_t i = 0; i < allAddresses.size(); ++i)
    {
      if (i != nodeId)
        {
          for (auto addr : allAddresses[i])
            {
              neighbors.push_back(addr.GetLocal());
            }
        }
    }

  for (uint32_t j = 0; j < node->GetNApplications(); ++j)
    {
      if (DynamicCast<UdpNeighborSender>(node->GetApplication(j)))
        {
          DynamicCast<UdpNeighborSender>(node->GetApplication(j))->m_neighbors = neighbors;
        }
    }
}

int
main(int argc, char* argv[])
{
  uint32_t numNodes = 10;
  double radius = 150.0;
  double simTime = 20.0;

  Config::SetDefault("ns3::AodvRoutingProtocol::HelloInterval", TimeValue(Seconds(1.0)));
  Config::SetDefault("ns3::AodvRoutingProtocol::RreqRetries", UintegerValue(2));

  NodeContainer nodes;
  nodes.Create(numNodes);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
  wifiPhy.SetChannel(wifiChannel.Create());

  WifiMacHelper wifiMac;
  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211a);
  wifi.SetRemoteStationManager("ns3::ArfWifiManager");

  wifiMac.SetType("ns3::AdhocWifiMac");
  NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

  AodvHelper aodv;
  OlsrHelper olsr;
  DsdvHelper dsdv;
  DsrHelper dsr;
  InternetStackHelper stack;
  stack.SetRoutingHelper(aodv);
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.0.0.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=300.0]"),
                                "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=300.0]"));
  mobility.SetMobilityModel("ns3::RandomWaypointMobilityModel",
                            "Speed", StringValue("ns3::ConstantRandomVariable[Constant=2.0]"),
                            "Pause", StringValue("ns3::ConstantRandomVariable[Constant=0.5]"));
  mobility.Install(nodes);

  std::vector<std::vector<Ipv4Address>> allAddresses;
  for (uint32_t i = 0; i < numNodes; ++i)
    {
      std::vector<Ipv4Address> addrs;
      for (uint32_t j = 0; j < interfaces.GetN(); ++j)
        {
          if (interfaces.GetAddress(j).GetLocal() == interfaces.GetAddress(j).GetLocal())
            {
              addrs.push_back(interfaces.GetAddress(j).GetLocal());
            }
        }
      allAddresses.push_back(addrs);
    }

  for (uint32_t i = 0; i < numNodes; ++i)
    {
      Ptr<Node> node = nodes.Get(i);
      Ptr<UdpNeighborSender> app = CreateObject<UdpNeighborSender>();
      app->SetAttribute("PacketSize", UintegerValue(512));
      app->SetAttribute("DataRate", DataRateValue(DataRate("100kbps")));
      node->AddApplication(app);
      UpdateNeighbors(node, interfaces, allAddresses, i);
    }

  for (double t = 1.0; t < simTime; t += 1.0)
    {
      Simulator::Schedule(Seconds(t), &UpdateNeighbors, nodes, interfaces, allAddresses, 0);
    }

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Simulator::Stop(Seconds(simTime));
  Simulator::Run();

  monitor->CheckForLostPackets();
  monitor->SerializeToXmlFile("manet-aodv.xml", false, false);

  Simulator::Destroy();
  return 0;
}