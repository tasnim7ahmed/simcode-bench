#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/olsr-module.h"
#include "ns3/energy-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WsnOlsrUdpExample");

// Application to simulate periodic sensor data transmission
class SensorApp : public Application
{
public:
  SensorApp() : m_socket(0), m_sendEvent(), m_running(false), m_interval(Seconds(2)){}

  void Setup(Address address, uint32_t packetSize, uint32_t nPackets, Time interval)
  {
    m_peerAddress = address;
    m_packetSize = packetSize;
    m_nPackets = nPackets;
    m_interval = interval;
  }

private:
  virtual void StartApplication()
  {
    m_running = true;
    m_packetsSent = 0;
    if (!m_socket)
    {
      m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
    }
    m_socket->Connect(m_peerAddress);
    SendPacket();
  }

  virtual void StopApplication()
  {
    m_running = false;
    if (m_sendEvent.IsRunning())
      Simulator::Cancel(m_sendEvent);

    if (m_socket)
      m_socket->Close();
  }

  void SendPacket()
  {
    Ptr<Packet> packet = Create<Packet>(m_packetSize);
    m_socket->Send(packet);

    ++m_packetsSent;
    if (m_packetsSent < m_nPackets && m_running)
    {
      m_sendEvent = Simulator::Schedule(m_interval, &SensorApp::SendPacket, this);
    }
  }

  Ptr<Socket> m_socket;
  Address m_peerAddress;
  uint32_t m_packetSize;
  uint32_t m_nPackets;
  uint32_t m_packetsSent;
  Time m_interval;
  EventId m_sendEvent;
  bool m_running;
};

int main(int argc, char *argv[])
{
  uint32_t nSensorNodes = 10;
  double simulationTime = 50.0; // seconds
  double txPowerStart = 3.0; // dBm
  double txPowerEnd = 3.0; // dBm
  uint32_t packetSize = 50; // bytes
  uint32_t packetsPerNode = 20; // per node
  double intervalSeconds = 2.0;

  CommandLine cmd;
  cmd.AddValue("nSensorNodes", "Number of sensor nodes", nSensorNodes);
  cmd.AddValue("simulationTime", "Simulation time [s]", simulationTime);
  cmd.Parse(argc, argv);

  nSensorNodes = std::max(nSensorNodes, 2u); // at least 2 nodes (1 sink + sensors)

  NodeContainer nodes;
  nodes.Create(nSensorNodes + 1); // Add one sink node

  // Assign names
  Names::Add("SinkNode", nodes.Get(0));

  // Install Wifi (IEEE 802.11b for low power WSN-like settings)
  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211b);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
  wifiPhy.Set("TxPowerStart", DoubleValue(txPowerStart));
  wifiPhy.Set("TxPowerEnd", DoubleValue(txPowerEnd));

  YansWifiChannelHelper wifiChannel;
  wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
  wifiChannel.AddPropagationLoss("ns3::LogDistancePropagationLossModel");
  wifiPhy.SetChannel(wifiChannel.Create());

  WifiMacHelper wifiMac;
  wifiMac.SetType("ns3::AdhocWifiMac");

  NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

  // Mobility: random for sensors, fixed for sink
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
  positionAlloc->Add(Vector(50.0, 50.0, 0.0)); // Sink at center
  for (uint32_t i = 0; i < nSensorNodes; ++i)
  {
    double x = 20 + 80 * ((double)rand() / RAND_MAX);
    double y = 20 + 80 * ((double)rand() / RAND_MAX);
    positionAlloc->Add(Vector(x, y, 0.0));
  }
  mobility.SetPositionAllocator(positionAlloc);
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(nodes);

  // Energy source and device energy model
  BasicEnergySourceHelper energySourceHelper;
  energySourceHelper.Set("BasicEnergySourceInitialEnergyJ", DoubleValue(1.0));
  EnergySourceContainer sources = energySourceHelper.Install(nodes);

  WifiRadioEnergyModelHelper radioEnergyHelper;
  DeviceEnergyModelContainer deviceModels = radioEnergyHelper.Install(devices, sources);

  // Internet stack
  InternetStackHelper internet;
  OlsrHelper olsr;
  internet.SetRoutingHelper(olsr);
  internet.Install(nodes);

  // Assign IP addresses
  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.0.0.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

  // Create UDP sink on the central node (node 0)
  uint16_t port = 9000;
  Address sinkAddress(InetSocketAddress(interfaces.GetAddress(0), port));

  PacketSinkHelper packetSinkHelper("ns3::UdpSocketFactory",
                                    InetSocketAddress(Ipv4Address::GetAny(), port));
  ApplicationContainer sinkApps = packetSinkHelper.Install(nodes.Get(0));
  sinkApps.Start(Seconds(0.0));
  sinkApps.Stop(Seconds(simulationTime));

  // Install SensorApp on each of the other nodes
  ApplicationContainer sensorApps;
  for (uint32_t i = 1; i <= nSensorNodes; ++i)
  {
    Ptr<SensorApp> app = CreateObject<SensorApp>();
    app->Setup(sinkAddress, packetSize, packetsPerNode, Seconds(intervalSeconds));
    nodes.Get(i)->AddApplication(app);
    app->SetStartTime(Seconds(1.0 + 0.1 * i)); // Stagger start
    app->SetStopTime(Seconds(simulationTime));
    sensorApps.Add(app);
  }

  // Enable tracing
  wifiPhy.EnablePcapAll("wsn-olsr-udp");
  AsciiTraceHelper ascii;
  wifiPhy.EnableAsciiAll(ascii.CreateFileStream("wsn-olsr-udp.tr"));

  // Print energy for each node at the end
  Simulator::Schedule(Seconds(simulationTime), [&sources](){
    for (uint32_t i = 0; i < sources.GetN(); ++i)
    {
      Ptr<EnergySource> src = sources.Get(i);
      std::cout << "Node " << i << " remaining energy: "
                << src->GetRemainingEnergy() << " J" << std::endl;
    }
  });

  Simulator::Stop(Seconds(simulationTime + 1.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}