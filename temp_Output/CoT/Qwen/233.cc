#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/udp-server-client-helper.h"
#include "ns3/application-container.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/config.h"
#include "ns3/string.h"
#include "ns3/ssid.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("UdpBroadcastSimulation");

class PacketSink : public Application
{
public:
  static TypeId GetTypeId()
  {
    return TypeId("PacketSink")
      .SetParent<Application>()
      .AddConstructor<PacketSink>();
  }

  PacketSink() {}
  virtual ~PacketSink() {}

private:
  virtual void StartApplication()
  {
    m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
    InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), 9);
    m_socket->Bind(local);
    m_socket->SetRecvCallback(MakeCallback(&PacketSink::Receive, this));
  }

  virtual void StopApplication()
  {
    if (m_socket)
    {
      m_socket->Close();
      m_socket = nullptr;
    }
  }

  void Receive(Ptr<Socket> socket)
  {
    Ptr<Packet> packet;
    Address from;
    while ((packet = socket->RecvFrom(from)))
    {
      NS_LOG_INFO(Simulator::Now().As(Time::S) << " Received packet size: " << packet->GetSize());
    }
  }

  Ptr<Socket> m_socket;
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

  WifiMacHelper mac;
  mac.SetType("ns3::AdhocWifiMac");

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy;
  phy.SetChannel(channel.Create());

  NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.0.0.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  // Set up broadcast for UDP packets
  for (uint32_t i = 1; i < nodes.GetN(); ++i)
  {
    PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetBroadcast(), 9));
    ApplicationContainer app = sink.Install(nodes.Get(i));
    app.Start(Seconds(0.0));
    app.Stop(Seconds(10.0));
  }

  // Sender application
  uint32_t packetSize = 1024;
  uint32_t maxPacketCount = 20;
  Time interPacketInterval = Seconds(1.0);

  Config::SetDefault("ns3::Ipv4L3Protocol::EnableBroadcast", BooleanValue(true));

  OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetBroadcast(), 9));
  onoff.SetConstantRate(DataRate("10kbps"));
  onoff.SetAttribute("PacketSize", UintegerValue(packetSize));
  onoff.SetAttribute("MaxBytes", UintegerValue(0)); // unlimited

  ApplicationContainer app = onoff.Install(nodes.Get(0));
  app.Start(Seconds(1.0));
  app.Stop(Seconds(10.0));

  Ipv4StaticRoutingHelper ipv4RoutingHelper;
  for (uint32_t i = 0; i < nodes.GetN(); ++i)
  {
    Ptr<Ipv4StaticRouting> routing = ipv4RoutingHelper.GetStaticRouting(nodes.Get(i)->GetObject<Ipv4>());
    routing->SetDefaultRoute(1, 0); // Send all non-local traffic via interface 1
  }

  Simulator::Run();
  Simulator::Destroy();
  return 0;
}