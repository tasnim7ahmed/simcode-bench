#include "ns3/core-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("VANETSimulation");

class BSMApplication : public Application
{
public:
  static TypeId GetTypeId();
  BSMApplication();
  virtual ~BSMApplication();

protected:
  virtual void StartApplication();
  virtual void StopApplication();

private:
  void SendPacket();
  uint32_t m_packetSize;
  Time m_interval;
  Ptr<Socket> m_socket;
  Address m_peer;
};

TypeId BSMApplication::GetTypeId()
{
  static TypeId tid = TypeId("BSMApplication")
                          .SetParent<Application>()
                          .AddConstructor<BSMApplication>()
                          .AddAttribute("Interval", "Time between sending packets",
                                        TimeValue(Seconds(0.1)),
                                        MakeTimeAccessor(&BSMApplication::m_interval),
                                        MakeTimeChecker())
                          .AddAttribute("PacketSize", "Size of each packet",
                                        UintegerValue(1024),
                                        MakeUintegerAccessor(&BSMApplication::m_packetSize),
                                        MakeUintegerChecker<uint32_t>());
  return tid;
}

BSMApplication::BSMApplication()
    : m_socket(nullptr), m_peer(Address()), m_interval(Seconds(0.1)), m_packetSize(1024)
{
}

BSMApplication::~BSMApplication()
{
  m_socket = nullptr;
}

void BSMApplication::StartApplication()
{
  if (!m_socket)
  {
    TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
    m_socket = Socket::CreateSocket(GetNode(), tid);
    m_socket->Bind();
    m_socket->Connect(m_peer);
  }

  Simulator::ScheduleNow(&BSMApplication::SendPacket, this);
}

void BSMApplication::StopApplication()
{
  if (m_socket)
  {
    m_socket->Close();
    m_socket = nullptr;
  }
}

void BSMApplication::SendPacket()
{
  Ptr<Packet> packet = Create<Packet>(m_packetSize);
  m_socket->Send(packet);

  Simulator::Schedule(m_interval, &BSMApplication::SendPacket, this);
}

int main(int argc, char *argv[])
{
  CommandLine cmd(__FILE__);
  cmd.Parse(argc, argv);

  Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue("2200"));

  NodeContainer nodes;
  nodes.Create(5);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  channel.AddPropagationLoss("ns3::NakagamiPropagationLossModel");

  YansWifiPhyHelper phy;
  phy.SetChannel(channel.Create());

  WifiMacHelper mac;
  WifiHelper wifi;

  wifi.SetStandard(WIFI_STANDARD_80211p);
  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                               "DataMode", StringValue("OfdmRate6MbpsBW10MHz"),
                               "ControlMode", StringValue("OfdmRate6MbpsBW10MHz"));

  mac.SetType("ns3::AdhocWifiMac");

  NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

  MobilityHelper mobility;
  mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
  mobility.Install(nodes);

  for (uint32_t i = 0; i < nodes.GetN(); ++i)
  {
    Ptr<Node> node = nodes.Get(i);
    Ptr<MobilityModel> mob = node->GetObject<MobilityModel>();
    Vector position = mob->GetPosition();
    NS_LOG_INFO("Vehicle " << i << " Position: " << position);
  }

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.0.0.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  for (uint32_t i = 0; i < nodes.GetN(); ++i)
  {
    for (uint32_t j = 0; j < nodes.GetN(); ++j)
    {
      if (i != j)
      {
        TypeId tid = TypeId::LookupByName("BSMApplication");
        ObjectFactory factory;
        factory.SetTypeId(tid);
        factory.Set("Interval", TimeValue(Seconds(0.1)));
        factory.Set("PacketSize", UintegerValue(1024));

        Ptr<Application> app = factory.Create<Application>();
        nodes.Get(i)->AddApplication(app);

        InetSocketAddress remoteAddr(interfaces.GetAddress(j), 80);
        app->GetObject<BSMApplication>()->m_peer = remoteAddr;
      }
    }
  }

  GlobalRoutingHelper::PopulateRoutingTables();

  AnimationInterface anim("vanet_simulation.xml");
  anim.EnablePacketMetadata(true);

  Simulator::Stop(Seconds(10));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}