#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/udp-client-server-module.h"
#include "ns3/packet-sink.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiBroadcast");

int main(int argc, char *argv[]) {

  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);
  LogComponent::SetMinimumLogLevel("WifiBroadcast", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create(4);

  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211b);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());

  WifiMacHelper mac;
  mac.SetType(WifiMacHelper::STA,
              "Ssid", Ssid("ns3-wifi"),
              "ActiveProbing", BooleanValue(false));

  NetDeviceContainer staDevices = wifi.Install(phy, mac, nodes.Get(1), nodes.Get(2), nodes.Get(3));

  mac.SetType(WifiMacHelper::AP,
              "Ssid", Ssid("ns3-wifi"),
              "BeaconInterval", TimeValue(Seconds(1.0)),
              "ProbeResponseWaitInterval", TimeValue(Seconds(0.1)));

  NetDeviceContainer apDevices = wifi.Install(phy, mac, nodes.Get(0));

  InternetStackHelper internet;
  internet.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(NetDeviceContainer::Create(apDevices, staDevices));

  UdpClientServerHelper broadcast(9);
  broadcast.SetAttribute("MaxPackets", UintegerValue(10));
  broadcast.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  broadcast.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps = broadcast.Install(nodes.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(12.0));

  for (uint32_t i = 1; i < 4; ++i) {
    Ptr<Node> receiver = nodes.Get(i);
    Ptr<Ipv4> ipv4 = receiver->GetObject<Ipv4>();
    Ipv4Address receiverAddress = ipv4->GetAddress(1, 0).GetLocal();

    TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
    Ptr<Socket> sink = Socket::CreateSocket(receiver, tid);
    InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), 9);
    sink->Bind(local);
    sink->SetRecvCallback(MakeCallback(&PacketSink::HandleRx, new PacketSink()));
  }

  Simulator::Stop(Seconds(15.0));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}