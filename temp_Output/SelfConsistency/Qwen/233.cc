#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("UdpBroadcastSimulation");

class UdpReceiver : public Application
{
public:
  static TypeId GetTypeId(void)
  {
    static TypeId tid = TypeId("UdpReceiver")
      .SetParent<Application>()
      .AddConstructor<UdpReceiver>();
    return tid;
  }

  UdpReceiver() {}
  virtual ~UdpReceiver() {}

private:
  virtual void StartApplication(void)
  {
    TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
    Ptr<Socket> recvSocket = Socket::CreateSocket(GetNode(), tid);
    InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), 9);
    recvSocket->Bind(local);
    recvSocket->SetRecvCallback(MakeCallback(&UdpReceiver::ReceivePacket, this));
    NS_LOG_INFO("Receiver socket bound to port 9");
  }

  virtual void StopApplication(void) {}

  void ReceivePacket(Ptr<Socket> socket)
  {
    Ptr<Packet> packet;
    Address from;
    while ((packet = socket->RecvFrom(from)))
    {
      NS_LOG_INFO("Received packet of size " << packet->GetSize() << " at time " << Simulator::Now().As(Time::S));
    }
  }
};

int main(int argc, char *argv[])
{
  CommandLine cmd(__FILE__);
  cmd.Parse(argc, argv);

  // Enable logging
  LogComponentEnable("UdpReceiver", LOG_LEVEL_INFO);
  LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

  // Create nodes
  NodeContainer nodes;
  nodes.Create(5); // 1 source, 4 receivers

  // Setup Wi-Fi
  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211a);

  YansWifiPhyHelper phy;
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  phy.SetChannel(channel.Create());

  NqosWifiMacHelper mac = NqosWifiMacHelper::Default();
  mac.SetType("ns3::AdhocWifiMac");

  NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

  // Install Internet stack
  InternetStackHelper stack;
  stack.Install(nodes);

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  // Set up broadcast
  TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
  Ptr<Socket> sourceSocket = Socket::CreateSocket(nodes.Get(0), tid);
  sourceSocket->SetAllowBroadcast(true);
  InetSocketAddress broadcastAddr = InetSocketAddress(Ipv4Address("255.255.255.255"), 9);
  sourceSocket->Connect(broadcastAddr);

  // Install receiver applications on all other nodes
  for (uint32_t i = 1; i < nodes.GetN(); ++i)
  {
    UdpReceiverHelper receiverHelper;
    receiverHelper.Install(nodes.Get(i));
  }

  // Send a packet at time 1s
  Simulator::Schedule(Seconds(1.0), &Socket::Send, sourceSocket, Create<Packet>(1024), 0);

  // Simulation run
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}