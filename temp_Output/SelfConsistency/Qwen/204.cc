#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/csma-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WirelessSensorNetworkSimulation");

class SensorNodeApplication : public Application
{
public:
  static TypeId GetTypeId(void);
  SensorNodeApplication();
  virtual ~SensorNodeApplication();

protected:
  virtual void StartApplication(void);
  virtual void StopApplication(void);

private:
  void SendPacket(void);
  Ptr<Socket> m_socket;
  Address m_peer;
  uint32_t m_packetSize;
  uint32_t m_packetsSent;
  DataRate m_dataRate;
  EventId m_sendEvent;
};

TypeId
SensorNodeApplication::GetTypeId(void)
{
  static TypeId tid = TypeId("SensorNodeApplication")
                          .SetParent<Application>()
                          .AddConstructor<SensorNodeApplication>();
  return tid;
}

SensorNodeApplication::SensorNodeApplication()
    : m_socket(0),
      m_peer(Address()),
      m_packetSize(1024),
      m_packetsSent(0),
      m_dataRate("1kbps"),
      m_sendEvent(EventId())
{
}

SensorNodeApplication::~SensorNodeApplication()
{
  m_socket = nullptr;
}

void SensorNodeApplication::StartApplication(void)
{
  m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
  m_socket->Connect(m_peer);
  SendPacket();
}

void SensorNodeApplication::StopApplication(void)
{
  if (m_sendEvent.IsRunning())
  {
    Simulator::Cancel(m_sendEvent);
  }

  if (m_socket)
  {
    m_socket->Close();
  }
}

void SensorNodeApplication::SendPacket(void)
{
  Ptr<Packet> packet = Create<Packet>(m_packetSize);
  m_socket->Send(packet);

  m_packetsSent++;
  Time interPacketInterval = Seconds((double)m_packetSize * 8 / m_dataRate.GetBitRate());
  m_sendEvent = Simulator::Schedule(interPacketInterval, &SensorNodeApplication::SendPacket, this);
}

int main(int argc, char *argv[])
{
  CommandLine cmd(__FILE__);
  cmd.Parse(argc, argv);

  LogComponentEnable("SensorNodeApplication", LOG_LEVEL_INFO);

  // Create nodes
  NodeContainer sensorNodes;
  sensorNodes.Create(5);
  NodeContainer baseStationNode;
  baseStationNode.Create(1);

  // Setup CSMA
  CsmaHelper csma;
  csma.SetChannelAttribute("DataRate", DataRateValue(DataRate("5Mbps")));
  csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

  NetDeviceContainer csmaDevices;
  csmaDevices = csma.Install(baseStationNode.Get(0));

  // Setup WiFi for sensors
  WifiMacHelper wifiMac;
  WifiPhyHelper wifiPhy;
  WifiHelper wifi;

  wifi.SetStandard(WIFI_STANDARD_80211b);

  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  phy.SetChannel(channel.Create());

  wifiMac.SetType("ns3::AdhocWifiMac");
  NetDeviceContainer wifiDevices = wifi.Install(phy, wifiMac, sensorNodes);

  // Mobility model
  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(20.0),
                                "DeltaY", DoubleValue(20.0),
                                "GridWidth", UintegerValue(3),
                                "LayoutType", StringValue("RowFirst"));

  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(sensorNodes);
  mobility.Install(baseStationNode);

  // Internet stack
  InternetStackHelper stack;
  stack.InstallAll();

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");

  Ipv4InterfaceContainer sensorInterfaces;
  sensorInterfaces = address.Assign(wifiDevices);

  Ipv4InterfaceContainer baseStationInterface;
  baseStationInterface = address.Assign(csmaDevices);

  // Install UDP server on base station
  UdpServerHelper server(9);
  ApplicationContainer serverApps = server.Install(baseStationNode.Get(0));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(20.0));

  // Install client applications on sensor nodes
  UdpClientHelper client(baseStationInterface.GetAddress(0), 9);
  client.SetAttribute("MaxPackets", UintegerValue(1000));
  client.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  client.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps = client.Install(sensorNodes);
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(20.0));

  // Set up NetAnim
  AnimationInterface anim("wsn-animation.xml");
  anim.SetConstantPosition(baseStationNode.Get(0), 60.0, 60.0);

  // Flow monitor setup
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Simulator::Stop(Seconds(20.0));
  Simulator::Run();

  // Output metrics
  monitor->CheckForLostPackets();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

  double totalDelay = 0;
  uint32_t totalReceived = 0;
  uint32_t totalSent = 0;

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i)
  {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
    if (t.destinationPort == 9)
    {
      NS_LOG_UNCOND("Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")");
      NS_LOG_UNCOND("  Tx Packets: " << i->second.txPackets);
      NS_LOG_UNCOND("  Rx Packets: " << i->second.rxPackets);
      NS_LOG_UNCOND("  Lost Packets: " << i->second.lostPackets);
      NS_LOG_UNCOND("  Avg Delay: " << i->second.delaySum.GetSeconds() / i->second.rxPackets << "s");

      totalSent += i->second.txPackets;
      totalReceived += i->second.rxPackets;
      totalDelay += i->second.delaySum.GetSeconds();
    }
  }

  double packetDeliveryRatio = (totalSent > 0) ? ((double)totalReceived / totalSent) : 0;
  double averageEndToEndDelay = (totalReceived > 0) ? (totalDelay / totalReceived) : 0;

  NS_LOG_UNCOND("=== Overall Metrics ===");
  NS_LOG_UNCOND("Packet Delivery Ratio: " << packetDeliveryRatio * 100 << "%");
  NS_LOG_UNCOND("Average End-to-End Delay: " << averageEndToEndDelay << " seconds");

  Simulator::Destroy();
  return 0;
}