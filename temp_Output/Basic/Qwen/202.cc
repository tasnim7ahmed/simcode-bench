#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/csma-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ieee802154-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WSNTopology");

class WSNApplication : public Application
{
public:
  static TypeId GetTypeId(void);
  WSNApplication();
  virtual ~WSNApplication();

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

TypeId WSNApplication::GetTypeId(void)
{
  static TypeId tid = TypeId("WSNApplication")
                          .SetParent<Application>()
                          .AddConstructor<WSNApplication>()
                          .AddAttribute("RemoteAddress", "The destination Address of the packets.",
                                        AddressValue(),
                                        MakeAddressAccessor(&WSNApplication::m_peer),
                                        MakeAddressChecker())
                          .AddAttribute("PacketSize", "Size of data packet to send in bytes.",
                                        UintegerValue(127),
                                        MakeUintegerAccessor(&WSNApplication::m_packetSize),
                                        MakeUintegerChecker<uint32_t>(1, 1024))
                          .AddAttribute("DataRate", "The data rate at which packets are sent.",
                                        DataRateValue(DataRate("1kbps")),
                                        MakeDataRateAccessor(&WSNApplication::m_dataRate),
                                        MakeDataRateChecker());
  return tid;
}

WSNApplication::WSNApplication()
    : m_socket(0), m_packetsSent(0)
{
}

WSNApplication::~WSNApplication()
{
  m_socket = nullptr;
}

void WSNApplication::StartApplication(void)
{
  if (!m_socket)
  {
    TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
    m_socket = Socket::CreateSocket(GetNode(), tid);
    m_socket->Connect(m_peer);
    m_socket->SetAllowBroadcast(true);
  }

  SendPacket();
}

void WSNApplication::StopApplication(void)
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

void WSNApplication::SendPacket(void)
{
  Ptr<Packet> packet = Create<Packet>(m_packetSize);
  m_socket->Send(packet);
  m_packetsSent++;

  Time interval = Seconds((double)m_packetSize * 8 / m_dataRate.GetBitRate());
  m_sendEvent = Simulator::Schedule(interval, &WSNApplication::SendPacket, this);
}

int main(int argc, char *argv[])
{
  CommandLine cmd(__FILE__);
  cmd.Parse(argc, argv);

  LogComponentEnable("WSNApplication", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create(6);

  // IEEE 802.15.4 channel and device setup
  Ieee802154Helper ieee802154 = Ieee802154Helper::Default();
  ieee802154.SetDeviceAttribute("Channel", UintegerValue(11)); // 2.4 GHz band

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
  wifiPhy.SetChannel(wifiChannel.Create());

  NetDeviceContainer devices = ieee802154.Install(nodes);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(10.0),
                                "DeltaY", DoubleValue(0.0),
                                "GridWidth", UintegerValue(6),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  uint16_t sinkPort = 9;
  Address sinkAddress(InetSocketAddress(interfaces.GetAddress(5), sinkPort));
  PacketSinkHelper packetSinkHelper("ns3::UdpSocketFactory",
                                    InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
  ApplicationContainer sinkApps = packetSinkHelper.Install(nodes.Get(5));
  sinkApps.Start(Seconds(0.0));
  sinkApps.Stop(Seconds(60.0));

  OnOffHelper onoff("ns3::UdpSocketFactory", Address());
  onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  onoff.SetAttribute("DataRate", DataRateValue(DataRate("1kbps")));
  onoff.SetAttribute("PacketSize", UintegerValue(127));

  for (uint32_t i = 0; i < 5; ++i)
  {
    onoff.SetAttribute("Remote", AddressValue(InetSocketAddress(interfaces.GetAddress(5), sinkPort)));
    ApplicationContainer apps = onoff.Install(nodes.Get(i));
    apps.Start(Seconds(1.0 + i));
    apps.Stop(Seconds(60.0));
  }

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  AnimationInterface anim("wsn-animation.xml");
  anim.EnablePacketMetadata(true);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  Simulator::Stop(Seconds(60.0));
  Simulator::Run();

  monitor->CheckForLostPackets();

  std::cout << "\n\nFlow Monitor Results:\n";
  monitor->SerializeToXmlStream(std::cout, 2, false, false);

  Simulator::Destroy();

  return 0;
}