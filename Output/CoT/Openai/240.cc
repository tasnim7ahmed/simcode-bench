#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-list-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiMulticastSimulation");

class MulticastReceiver : public Application
{
public:
  MulticastReceiver() : m_socket(0) {}
  virtual ~MulticastReceiver() { m_socket = 0; }

  void Setup(Address address, uint16_t port)
  {
    m_local = InetSocketAddress(Ipv4Address::GetAny(), port);
    m_multicast = InetSocketAddress(InetSocketAddress::ConvertFrom(address).GetIpv4(), port);
    m_port = port;
  }

private:
  virtual void StartApplication() override
  {
    if (!m_socket)
    {
      m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
      m_socket->Bind(m_local);
      m_socket->SetRecvCallback(MakeCallback(&MulticastReceiver::HandleRead, this));
      // Join multicast group
      Ptr<Ipv4> ipv4 = GetNode()->GetObject<Ipv4>();
      Ipv4Address multicastGroup = m_multicast.GetIpv4();
      Ipv4Address interface = ipv4->GetAddress(1, 0).GetLocal();
      ipv4->GetObject<Ipv4L3Protocol>()->JoinGroup(1, multicastGroup);
    }
  }

  virtual void StopApplication() override
  {
    if (m_socket)
    {
      m_socket->Close();
    }
  }

  void HandleRead(Ptr<Socket> socket)
  {
    Ptr<Packet> packet;
    Address from;
    while ((packet = socket->RecvFrom(from)))
    {
      NS_LOG_UNCOND("At " << Simulator::Now().GetSeconds() << "s, "
                    << GetNode()->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal()
                    << " received " << packet->GetSize() << " bytes from "
                    << InetSocketAddress::ConvertFrom(from).GetIpv4());
    }
  }

  Ptr<Socket> m_socket;
  InetSocketAddress m_local;
  InetSocketAddress m_multicast;
  uint16_t m_port;
};

int main(int argc, char *argv[])
{
  uint32_t nReceivers = 3;
  double simulationTime = 10.0;
  uint16_t multicastPort = 5000;
  std::string multicastGroup = "225.1.2.3";

  CommandLine cmd;
  cmd.AddValue("nReceivers", "Number of multicast receivers", nReceivers);
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(nReceivers + 1); // 1 sender + N receivers

  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211g);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
  wifiPhy.Set("TxPowerStart", DoubleValue(20.0));
  wifiPhy.Set("TxPowerEnd", DoubleValue(20.0));
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
  wifiPhy.SetChannel(wifiChannel.Create());

  WifiMacHelper wifiMac;
  wifiMac.SetType("ns3::AdhocWifiMac");

  NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
  positionAlloc->Add(Vector(0.0, 0.0, 0.0)); // Sender
  for (uint32_t i = 0; i < nReceivers; ++i)
  {
    positionAlloc->Add(Vector(10.0 + 10.0*i, 0.0, 0.0)); // Receivers
  }
  mobility.SetPositionAllocator(positionAlloc);
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(nodes);

  InternetStackHelper internet;
  Ipv4StaticRoutingHelper staticRouting;
  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  // Set up sender application
  Ptr<Socket> senderSocket = Socket::CreateSocket(nodes.Get(0), UdpSocketFactory::GetTypeId());

  // Configure socket for multicast
  senderSocket->SetAllowBroadcast(true);

  // OnOff application to generate UDP traffic
  OnOffHelper onoff("ns3::UdpSocketFactory", Address(InetSocketAddress(Ipv4Address(multicastGroup.c_str()), multicastPort)));
  onoff.SetAttribute("DataRate", StringValue("2Mbps"));
  onoff.SetAttribute("PacketSize", UintegerValue(512));
  onoff.SetAttribute("StartTime", TimeValue(Seconds(1.0)));
  onoff.SetAttribute("StopTime", TimeValue(Seconds(simulationTime - 1)));

  ApplicationContainer appSender = onoff.Install(nodes.Get(0));

  // Set up receiver applications on all receiver nodes
  for (uint32_t i = 1; i <= nReceivers; ++i)
  {
    Ptr<MulticastReceiver> app = CreateObject<MulticastReceiver>();
    app->Setup(Ipv4Address(multicastGroup.c_str()), multicastPort);
    nodes.Get(i)->AddApplication(app);
    app->SetStartTime(Seconds(0.5));
    app->SetStopTime(Seconds(simulationTime));
  }

  // Receivers join multicast group
  for (uint32_t i = 1; i <= nReceivers; ++i)
  {
    Ptr<Ipv4> ipv4 = nodes.Get(i)->GetObject<Ipv4>();
    Ipv4Address multicast = Ipv4Address(multicastGroup.c_str());
    ipv4->SetMulticastLoopback(true);
    staticRouting.SetDefaultMulticastRoute(nodes.Get(i), interfaces.Get(i), 1);
    ipv4->GetObject<Ipv4L3Protocol>()->JoinGroup(1, multicast);
  }

  // Enable packet capture
  wifiPhy.EnablePcapAll("wifi-multicast");

  Simulator::Stop(Seconds(simulationTime));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}