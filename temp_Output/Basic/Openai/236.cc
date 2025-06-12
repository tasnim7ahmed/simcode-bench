#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/udp-client-server-helper.h"

using namespace ns3;

class UdpServerWithLogging : public Application
{
public:
  UdpServerWithLogging() : m_socket(0), m_received(0) {}
  virtual ~UdpServerWithLogging() { m_socket = 0; }

  void Setup(Address address)
  {
    m_local = address;
  }

private:
  virtual void StartApplication()
  {
    if (!m_socket)
    {
      m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
      m_socket->Bind(m_local);
      m_socket->SetRecvCallback(MakeCallback(&UdpServerWithLogging::HandleRead, this));
    }
  }

  virtual void StopApplication()
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
      ++m_received;
      NS_LOG_UNCOND("Time " << Simulator::Now().GetSeconds() 
        << "s, AP received UDP packet of " << packet->GetSize() 
        << " bytes from " << InetSocketAddress::ConvertFrom(from).GetIpv4());
    }
  }

  Ptr<Socket> m_socket;
  Address m_local;
  uint32_t m_received;
};

int main(int argc, char *argv[])
{
  Time::SetResolution(Time::NS);

  NodeContainer wifiStaNode;
  wifiStaNode.Create(1);
  NodeContainer wifiApNode;
  wifiApNode.Create(1);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());

  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211n_2_4GHZ);
  WifiMacHelper mac;

  Ssid ssid = Ssid("ns3-ssid");
  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid),
              "ActiveProbing", BooleanValue(false));
  NetDeviceContainer staDevice;
  staDevice = wifi.Install(phy, mac, wifiStaNode);

  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid));
  NetDeviceContainer apDevice;
  apDevice = wifi.Install(phy, mac, wifiApNode);

  MobilityHelper mobility;
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(wifiStaNode);
  mobility.Install(wifiApNode);

  InternetStackHelper stack;
  stack.Install(wifiApNode);
  stack.Install(wifiStaNode);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer staIface = address.Assign(staDevice);
  Ipv4InterfaceContainer apIface = address.Assign(apDevice);

  uint16_t serverPort = 4000;
  Ptr<UdpServerWithLogging> serverApp = CreateObject<UdpServerWithLogging>();
  Address apLocal = InetSocketAddress(Ipv4Address::GetAny(), serverPort);
  serverApp->Setup(apLocal);
  wifiApNode.Get(0)->AddApplication(serverApp);
  serverApp->SetStartTime(Seconds(1.0));
  serverApp->SetStopTime(Seconds(10.0));

  UdpClientHelper client(apIface.GetAddress(0), serverPort);
  client.SetAttribute("MaxPackets", UintegerValue(20));
  client.SetAttribute("Interval", TimeValue(Seconds(0.5)));
  client.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientApp = client.Install(wifiStaNode.Get(0));
  clientApp.Start(Seconds(2.0));
  clientApp.Stop(Seconds(10.0));

  Simulator::Stop(Seconds(11.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}