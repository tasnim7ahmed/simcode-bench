#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiUdpClientServer");

class UdpServerLogger : public Application
{
public:
  UdpServerLogger() {}
  virtual ~UdpServerLogger() {}

  void Setup(Address address, uint16_t port)
  {
    m_local = InetSocketAddress(Ipv4Address::GetAny(), port);
  }

private:
  virtual void StartApplication()
  {
    m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
    m_socket->Bind(m_local);
    m_socket->SetRecvCallback(MakeCallback(&UdpServerLogger::HandleRead, this));
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
    Ptr<Packet> packet;
    Address from;
    while ((packet = socket->RecvFrom(from)))
      {
        NS_LOG_UNCOND("Server received " << packet->GetSize()
                         << " bytes from " << InetSocketAddress::ConvertFrom(from).GetIpv4());
      }
  }

  Ptr<Socket> m_socket;
  InetSocketAddress m_local;
};

int main(int argc, char *argv[])
{
  uint32_t nClients = 4;
  uint16_t servPort = 4000;
  double simTime = 5.0; // seconds

  CommandLine cmd;
  cmd.AddValue("nClients", "Number of WiFi clients", nClients);
  cmd.Parse(argc, argv);

  NodeContainer wifiStaNodes;
  wifiStaNodes.Create(nClients);
  NodeContainer wifiApNode;
  wifiApNode.Create(1);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());

  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211b);
  wifi.SetRemoteStationManager("ns3::AarfWifiManager");

  WifiMacHelper mac;
  Ssid ssid = Ssid("ns3-wifi");

  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid),
              "ActiveProbing", BooleanValue(false));
  NetDeviceContainer staDevices = wifi.Install(phy, mac, wifiStaNodes);

  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid));
  NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiApNode);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                               "MinX", DoubleValue(0.0),
                               "MinY", DoubleValue(0.0),
                               "DeltaX", DoubleValue(5.0),
                               "DeltaY", DoubleValue(5.0),
                               "GridWidth", UintegerValue(3),
                               "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");

  mobility.Install(wifiStaNodes);
  mobility.Install(wifiApNode);

  InternetStackHelper stack;
  stack.Install(wifiStaNodes);
  stack.Install(wifiApNode);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer staInterfaces;
  staInterfaces = address.Assign(staDevices);
  Ipv4InterfaceContainer apInterface;
  apInterface = address.Assign(apDevice);

  Ptr<Node> serverNode = wifiApNode.Get(0);
  Ptr<UdpServerLogger> serverApp = CreateObject<UdpServerLogger>();
  serverApp->Setup(InetSocketAddress(apInterface.GetAddress(0), servPort), servPort);
  serverNode->AddApplication(serverApp);
  serverApp->SetStartTime(Seconds(0.0));
  serverApp->SetStopTime(Seconds(simTime));

  for (uint32_t i = 0; i < nClients; ++i)
    {
      Ptr<Socket> srcSocket = Socket::CreateSocket(wifiStaNodes.Get(i), UdpSocketFactory::GetTypeId());
      Address dstAddress = InetSocketAddress(apInterface.GetAddress(0), servPort);

      Ptr<UniformRandomVariable> uv = CreateObject<UniformRandomVariable>();
      double start = uv->GetValue(0.1, 0.5);

      Simulator::Schedule(Seconds(start), [srcSocket, dstAddress]()
        {
          Ptr<Packet> packet = Create<Packet>(1024);
          srcSocket->SendTo(packet, 0, dstAddress);
        });

      // send additional packets per client
      for (uint32_t pkt = 1; pkt < 5; ++pkt)
        {
          Simulator::Schedule(Seconds(start + pkt * 0.8), [srcSocket, dstAddress]()
            {
              Ptr<Packet> packet = Create<Packet>(1024);
              srcSocket->SendTo(packet, 0, dstAddress);
            });
        }
    }

  Simulator::Stop(Seconds(simTime));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}