#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/ssid.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiUdpBroadcast");

class ReceiverApp : public Application
{
public:
  ReceiverApp() {}
  virtual ~ReceiverApp() {}
  void Setup(Address address, uint16_t port)
  {
    m_listenAddress = address;
    m_port = port;
  }

private:
  virtual void StartApplication() override
  {
    TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
    m_socket = Socket::CreateSocket(GetNode(), tid);
    InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), m_port);
    m_socket->Bind(local);
    m_socket->SetRecvCallback(MakeCallback(&ReceiverApp::HandleRead, this));
  }

  virtual void StopApplication() override
  {
    if (m_socket)
    {
      m_socket->Close();
      m_socket = 0;
    }
  }

  void HandleRead(Ptr<Socket> socket)
  {
    Address from;
    while (Ptr<Packet> packet = socket->RecvFrom(from))
    {
      if (packet->GetSize() > 0)
      {
        NS_LOG_INFO("At " << Simulator::Now().GetSeconds()
                           << "s Node " << GetNode()->GetId()
                           << " received packet of " << packet->GetSize()
                           << " bytes from " << InetSocketAddress::ConvertFrom(from).GetIpv4());
      }
    }
  }

  Ptr<Socket> m_socket;
  Address m_listenAddress;
  uint16_t m_port;
};

int main(int argc, char *argv[])
{
  LogComponentEnable("WifiUdpBroadcast", LOG_LEVEL_INFO);

  uint32_t nReceivers = 4;
  uint16_t port = 4000;
  double simulationTime = 5.0;

  NodeContainer wifiNodes;
  wifiNodes.Create(1 + nReceivers); // 1 sender + n receivers

  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  phy.SetChannel(channel.Create());

  WifiMacHelper mac;
  Ssid ssid = Ssid("simple-wifi");

  mac.SetType("ns3::AdhocWifiMac", "Ssid", SsidValue(ssid));
  NetDeviceContainer devices = wifi.Install(phy, mac, wifiNodes);

  InternetStackHelper internet;
  internet.Install(wifiNodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer ifs = ipv4.Assign(devices);

  // Install receiver applications
  for (uint32_t i = 1; i <= nReceivers; ++i)
  {
    Ptr<ReceiverApp> app = CreateObject<ReceiverApp>();
    app->Setup(ifs.GetAddress(i), port);
    wifiNodes.Get(i)->AddApplication(app);
    app->SetStartTime(Seconds(0.0));
    app->SetStopTime(Seconds(simulationTime));
  }

  // Install UDP broadcast sender
  Ptr<Socket> srcSocket = Socket::CreateSocket(wifiNodes.Get(0), UdpSocketFactory::GetTypeId());
  srcSocket->SetAllowBroadcast(true);

  Simulator::ScheduleWithContext(srcSocket->GetNode()->GetId(), Seconds(1.0),
                                 [&]()
                                 {
                                   Ipv4Address broadcastAddr("10.1.1.255");
                                   InetSocketAddress remote = InetSocketAddress(broadcastAddr, port);
                                   srcSocket->Connect(remote);
                                   for (uint32_t i = 0; i < 10; ++i)
                                   {
                                     Simulator::Schedule(Seconds(1.0 + 0.2 * i),
                                                        [srcSocket]()
                                                        {
                                                          Ptr<Packet> packet = Create<Packet>(100);
                                                          srcSocket->Send(packet);
                                                        });
                                   }
                                 });

  Simulator::Stop(Seconds(simulationTime));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}