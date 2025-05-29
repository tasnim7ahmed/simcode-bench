#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

class UdpPacketLogger : public Application
{
public:
  UdpPacketLogger() {}
  virtual ~UdpPacketLogger() {}

protected:
  virtual void StartApplication() override
  {
    if (!m_socket)
    {
      m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
      InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), 9);
      m_socket->Bind(local);
      m_socket->SetRecvCallback(MakeCallback(&UdpPacketLogger::HandleRead, this));
    }
  }

  virtual void StopApplication() override
  {
    if (m_socket)
    {
      m_socket->Close();
      m_socket->SetRecvCallback(MakeNullCallback<void, Ptr<Socket>>());
    }
  }

  void HandleRead(Ptr<Socket> socket)
  {
    Ptr<Packet> packet;
    Address from;
    while ((packet = socket->RecvFrom(from)))
    {
      NS_LOG_UNCOND("AP Received packet of size " << packet->GetSize() << " bytes from " << InetSocketAddress::ConvertFrom(from).GetIpv4());
    }
  }

private:
  Ptr<Socket> m_socket;
};

int main(int argc, char *argv[])
{
  Time::SetResolution(Time::NS);
  LogComponentEnable("UdpPacketLogger", LOG_LEVEL_INFO);

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
  Ssid ssid = Ssid("ns3-wifi");

  // STA
  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid),
              "ActiveProbing", BooleanValue(false));
  NetDeviceContainer staDevice;
  staDevice = wifi.Install(phy, mac, wifiStaNode);

  // AP
  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid));
  NetDeviceContainer apDevice;
  apDevice = wifi.Install(phy, mac, wifiApNode);

  MobilityHelper mobility;
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(wifiStaNode);
  mobility.Install(wifiApNode);
  wifiApNode.Get(0)->GetObject<MobilityModel>()->SetPosition(Vector(0.0, 0.0, 0.0));
  wifiStaNode.Get(0)->GetObject<MobilityModel>()->SetPosition(Vector(5.0, 0.0, 0.0));

  InternetStackHelper stack;
  stack.Install(wifiStaNode);
  stack.Install(wifiApNode);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer staInterface, apInterface;
  staInterface = address.Assign(staDevice);
  apInterface = address.Assign(apDevice);

  uint16_t port = 9;
  Ptr<UdpPacketLogger> serverApp = CreateObject<UdpPacketLogger>();
  wifiApNode.Get(0)->AddApplication(serverApp);
  serverApp->SetStartTime(Seconds(1.0));
  serverApp->SetStopTime(Seconds(10.0));

  UdpClientHelper client(apInterface.GetAddress(0), port);
  client.SetAttribute("MaxPackets", UintegerValue(10));
  client.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  client.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientApp = client.Install(wifiStaNode.Get(0));
  clientApp.Start(Seconds(2.0));
  clientApp.Stop(Seconds(9.0));

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}