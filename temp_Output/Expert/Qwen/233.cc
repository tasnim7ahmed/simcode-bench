#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiBroadcastSimulation");

class UdpReceiver : public Application
{
public:
  static TypeId GetTypeId(void)
  {
    static TypeId tid = TypeId("UdpReceiver")
      .SetParent<Application>()
      .SetGroupName("Applications")
      .AddConstructor<UdpReceiver>();
    return tid;
  }

  UdpReceiver() {}
  virtual ~UdpReceiver() {}

private:
  virtual void StartApplication(void)
  {
    TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
    Ptr<Socket> socket = Socket::CreateSocket(GetNode(), tid);
    InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), 9);
    socket->Bind(local);
    socket->SetRecvCallback(MakeCallback(&UdpReceiver::ReceivePacket, this));
  }

  virtual void StopApplication(void) {}

  void ReceivePacket(Ptr<Socket> socket)
  {
    Ptr<Packet> packet;
    Address from;
    while ((packet = socket->RecvFrom(from)))
    {
      NS_LOG_INFO("Received packet of size " << packet->GetSize() << " at " << Simulator::Now().As(Time::S));
    }
  }
};

int main(int argc, char *argv[])
{
  CommandLine cmd(__FILE__);
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(5); // One sender, four receivers

  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211a);
  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue("OfdmRate6Mbps"));

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy;
  phy.SetChannel(channel.Create());

  WifiMacHelper mac;
  mac.SetType("ns3::AdhocWifiMac");

  NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.0.0.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  // Set up applications
  UdpServerHelper server(9);
  ApplicationContainer receiverApps;

  for (uint32_t i = 1; i < nodes.GetN(); ++i)
  {
    receiverApps.Add(server.Install(nodes.Get(i)));
  }

  receiverApps.Start(Seconds(1.0));
  receiverApps.Stop(Seconds(10.0));

  // Custom client app to send broadcast packets
  TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
  Ptr<Socket> sourceSocket = Socket::CreateSocket(nodes.Get(0), tid);
  InetSocketAddress destAddr = InetSocketAddress(Ipv4Address("255.255.255.255"), 9);
  sourceSocket->SetAllowBroadcast(true);

  Simulator::Schedule(Seconds(2.0), &Socket::SendTo, sourceSocket, Create<Packet>(1024), 0, destAddr);
  Simulator::Schedule(Seconds(5.0), &Socket::SendTo, sourceSocket, Create<Packet>(1024), 0, destAddr);

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}