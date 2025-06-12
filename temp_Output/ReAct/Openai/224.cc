#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/aodv-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ManetAodvSimulation");

const uint32_t numNodes = 10;
const double simTime = 20.0; // seconds
const double packetInterval = 1.0; // seconds
const uint16_t udpPort = 5000;
const uint32_t areaSize = 200;

Ptr<UniformRandomVariable> uv = CreateObject<UniformRandomVariable>();

// Custom application to send to a random neighbor
class PeriodicRandomUdpApp : public Application
{
public:
  PeriodicRandomUdpApp() {}
  virtual ~PeriodicRandomUdpApp() { m_socket = 0; }

  void Setup(Ptr<Socket> socket, Ipv4InterfaceContainer* interfaces, uint32_t myId)
  {
    m_socket = socket;
    m_interfaces = interfaces;
    m_myId = myId;
  }

  void StartApplication() override
  {
    m_running = true;
    m_socket->Bind();
    ScheduleNextTx();
  }

  void StopApplication() override
  {
    m_running = false;
    if (m_sendEvent.IsRunning()) Simulator::Cancel(m_sendEvent);
    if (m_socket) m_socket->Close();
  }

private:
  void ScheduleNextTx()
  {
    if (m_running)
      m_sendEvent = Simulator::Schedule(Seconds(packetInterval), &PeriodicRandomUdpApp::SendPacket, this);
  }

  void SendPacket()
  {
    // Pick a random neighbor excluding self
    uint32_t totalNodes = m_interfaces->GetN();
    if (totalNodes < 2) return;
    uint32_t dstId;
    do {
      dstId = uv->GetInteger(0, totalNodes - 1);
    } while (dstId == m_myId);

    Ipv4Address dstAddr = m_interfaces->GetAddress(dstId);
    Ptr<Packet> p = Create<Packet>(100);

    m_socket->SendTo(p, 0, InetSocketAddress(dstAddr, udpPort));

    ScheduleNextTx();
  }

  Ptr<Socket> m_socket;
  Ipv4InterfaceContainer* m_interfaces;
  uint32_t m_myId;
  bool m_running = false;
  EventId m_sendEvent;
};

int main(int argc, char* argv[])
{
  LogComponentEnable("ManetAodvSimulation", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create(numNodes);

  // WiFi
  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211b);
  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
  wifiPhy.SetChannel(wifiChannel.Create());

  WifiMacHelper wifiMac;
  wifiMac.SetType("ns3::AdhocWifiMac");

  NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

  // Mobility: Random Waypoint  
  MobilityHelper mobility;
  mobility.SetMobilityModel(
    "ns3::RandomWaypointMobilityModel",
    "Speed", StringValue("ns3::UniformRandomVariable[Min=1.0|Max=5.0]"),
    "Pause", StringValue("ns3::ConstantRandomVariable[Constant=0.5]"),
    "PositionAllocator", PointerValue(
        CreateObjectWithAttributes<RandomRectanglePositionAllocator>(
          "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=200.0]"),
          "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=200.0]"))));
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                               "MinX", DoubleValue(0.0),
                               "MinY", DoubleValue(0.0),
                               "DeltaX", DoubleValue(20.0),
                               "DeltaY", DoubleValue(20.0),
                               "GridWidth", UintegerValue(5),
                               "LayoutType", StringValue("RowFirst"));
  mobility.Install(nodes);

  // Internet stack with AODV
  InternetStackHelper internet;
  AodvHelper aodv;
  internet.SetRoutingHelper(aodv);
  internet.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.0.0", "255.255.0.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  // UDP sockets & custom periodic random sender apps
  for (uint32_t i = 0; i < numNodes; ++i)
  {
    Ptr<Socket> srcSocket = Socket::CreateSocket(nodes.Get(i), UdpSocketFactory::GetTypeId());

    Ptr<PeriodicRandomUdpApp> app = CreateObject<PeriodicRandomUdpApp>();
    app->Setup(srcSocket, &interfaces, i);
    nodes.Get(i)->AddApplication(app);
    app->SetStartTime(Seconds(uv->GetValue(0.1, 0.5))); // slight random offset
    app->SetStopTime(Seconds(simTime));
  }

  // UDP packet sink on all nodes (receives on port udpPort)
  for (uint32_t i = 0; i < numNodes; ++i)
  {
    PacketSinkHelper sinkHelper("ns3::UdpSocketFactory",
        InetSocketAddress(Ipv4Address::GetAny(), udpPort));
    ApplicationContainer sinks = sinkHelper.Install(nodes.Get(i));
    sinks.Start(Seconds(0));
    sinks.Stop(Seconds(simTime));
  }

  // Enable PCAP tracing
  wifiPhy.EnablePcapAll("manet-aodv");

  Simulator::Stop(Seconds(simTime));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}