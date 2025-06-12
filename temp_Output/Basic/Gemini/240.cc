#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/netanim-module.h"
#include "ns3/mobility-module.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiMulticast");

int main(int argc, char *argv[]) {

  bool verbose = false;
  bool tracing = false;
  uint32_t nReceiver = 3;

  CommandLine cmd;
  cmd.AddValue("verbose", "Tell echo applications to log if true", verbose);
  cmd.AddValue("tracing", "Enable pcap tracing", tracing);
  cmd.AddValue("nReceiver", "Number of receiver nodes", nReceiver);

  cmd.Parse(argc, argv);

  if (verbose) {
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
  }

  NodeContainer apNode;
  apNode.Create(1);

  NodeContainer receiverNodes;
  receiverNodes.Create(nReceiver);

  NodeContainer senderNode;
  senderNode.Create(1);

  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211n);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());

  WifiMacHelper mac;
  Ssid ssid = Ssid("ns-3-ssid");
  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid));

  NetDeviceContainer apDevice;
  apDevice = wifi.Install(phy, mac, apNode);

  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid),
              "ActiveProbing", BooleanValue(false));

  NetDeviceContainer receiverDevices;
  receiverDevices = wifi.Install(phy, mac, receiverNodes);

  NetDeviceContainer senderDevice;
  senderDevice = wifi.Install(phy, mac, senderNode);

  MobilityHelper mobility;

  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(5.0),
                                "DeltaY", DoubleValue(10.0),
                                "GridWidth", UintegerValue(3),
                                "LayoutType", StringValue("RowFirst"));

  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(receiverNodes);
  mobility.Install(senderNode);

  mobility.SetPositionAllocator("ns3::RandomDiscPositionAllocator",
                                "X", DoubleValue(50.0),
                                "Y", DoubleValue(50.0),
                                "Rho", StringValue("ns3::UniformRandomVariable[Min=0|Max=5]"));

  mobility.Install(apNode);

  InternetStackHelper internet;
  internet.Install(apNode);
  internet.Install(receiverNodes);
  internet.Install(senderNode);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterface;
  apInterface = address.Assign(apDevice);
  Ipv4InterfaceContainer receiverInterface;
  receiverInterface = address.Assign(receiverDevices);
  Ipv4InterfaceContainer senderInterface;
  senderInterface = address.Assign(senderDevice);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  std::string multicastAddress = "225.1.2.3";
  Address multicastGroupAddress;
  multicastGroupAddress = InetSocketAddress{Ipv4Address{multicastAddress.c_str()}, 9};

  UdpClientHelper client(Ipv4Address(multicastAddress.c_str()), 9);
  client.SetAttribute("MaxPackets", UintegerValue(10));
  client.SetAttribute("Interval", TimeValue(Seconds(1.)));
  client.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps = client.Install(senderNode.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  UdpServerHelper server(9);
  ApplicationContainer serverApps = server.Install(receiverNodes);
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(11.0));

  for (uint32_t i = 0; i < nReceiver; ++i) {
      Ptr<Node> receiver = receiverNodes.Get(i);
      Ptr<Ipv4> ipv4 = receiver->GetObject<Ipv4>();
      ipv4->MulticastEnable();
      ipv4->AddMulticastRoute(Ipv4Address(multicastAddress.c_str()), 0);
  }

    Ptr<Node> ap = apNode.Get(0);
    Ptr<Ipv4> apIpv4 = ap->GetObject<Ipv4>();
    apIpv4->MulticastEnable();
    apIpv4->AddMulticastRoute(Ipv4Address(multicastAddress.c_str()), 0);

    Ptr<Node> sender = senderNode.Get(0);
    Ptr<Ipv4> senderIpv4 = sender->GetObject<Ipv4>();
    senderIpv4->MulticastEnable();

  if (tracing == true) {
    phy.EnablePcapAll("wifi-multicast");
  }

  AnimationInterface anim("wifi-multicast.xml");
  anim.SetConstantPosition(apNode.Get(0), 50.0, 50.0);

  Simulator::Stop(Seconds(12.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}