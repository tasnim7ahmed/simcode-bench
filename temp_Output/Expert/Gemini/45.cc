#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/packet-sink.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/on-off-application.h"
#include "ns3/on-off-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  bool verbose = false;
  bool tracing = false;
  double primaryRss = -80;
  double interferenceRss = -70;
  double timeOffset = 0.000001;
  uint32_t primaryPacketSize = 1024;
  uint32_t interferencePacketSize = 512;

  CommandLine cmd;
  cmd.AddValue("verbose", "Tell application to log if true", verbose);
  cmd.AddValue("tracing", "Enable pcap tracing", tracing);
  cmd.AddValue("primaryRss", "Rx RSS for primary packet (dBm)", primaryRss);
  cmd.AddValue("interferenceRss", "Rx RSS for interference packet (dBm)", interferenceRss);
  cmd.AddValue("timeOffset", "Time offset between primary and interference packets (seconds)", timeOffset);
  cmd.AddValue("primaryPacketSize", "Size of primary packet (bytes)", primaryPacketSize);
  cmd.AddValue("interferencePacketSize", "Size of interference packet (bytes)", interferencePacketSize);
  cmd.Parse(argc, argv);

  if (verbose) {
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
  }

  NodeContainer nodes;
  nodes.Create(3);

  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
  wifiPhy.SetChannel(wifiChannel.Create());

  wifiPhy.Set("RxGain", DoubleValue(0));

  WifiMacHelper wifiMac;
  Ssid ssid = Ssid("ns-3-ssid");
  wifiMac.SetType("ns3::StaWifiMac",
                  "Ssid", SsidValue(ssid),
                  "ActiveProbing", BooleanValue(false));

  NetDeviceContainer staDevices;
  staDevices = wifi.Install(wifiPhy, wifiMac, nodes.Get(0));
  staDevices.Add(wifi.Install(wifiPhy, wifiMac, nodes.Get(1)));

  wifiMac.SetType("ns3::ApWifiMac",
                  "Ssid", SsidValue(ssid));

  NetDeviceContainer apDevices;
  apDevices = wifi.Install(wifiPhy, wifiMac, nodes.Get(2));

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(5.0),
                                "DeltaY", DoubleValue(10.0),
                                "GridWidth", UintegerValue(3),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(nodes);

  InternetStackHelper internet;
  internet.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(NetDeviceContainer::Create(staDevices, apDevices));

  PacketSinkHelper sinkHelper("ns3::UdpSocketFactory", InetSocketAddress(interfaces.GetAddress(0), 9));
  ApplicationContainer sinkApp = sinkHelper.Install(nodes.Get(0));
  sinkApp.Start(Seconds(1.0));
  sinkApp.Stop(Seconds(10.0));

  OnOffHelper onOffHelper("ns3::UdpSocketFactory", InetSocketAddress(interfaces.GetAddress(0), 9));
  onOffHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  onOffHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  onOffHelper.SetAttribute("PacketSize", UintegerValue(primaryPacketSize));
  ApplicationContainer onOffApp = onOffHelper.Install(nodes.Get(1));
  onOffApp.Start(Seconds(2.0));
  onOffApp.Stop(Seconds(10.0));

  Ptr<Node> interNode = nodes.Get(2);
  Ptr<NetDevice> apDevice = apDevices.Get(0);

  Simulator::Schedule(Seconds(2.0 + timeOffset), [interNode, apDevice, interferencePacketSize, interfaces, interferenceRss]() {
      TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
      Ptr<Socket> socket = Socket::CreateSocket(interNode, tid);
      InetSocketAddress remote = InetSocketAddress(interfaces.GetAddress(0), 9);
      socket->Connect(remote);
      socket->SetAllowBroadcast(true);
      Ptr<Packet> packet = Create<Packet>(interferencePacketSize);
      socket->Send(packet);

      Ptr<WifiNetDevice> wifiNetDevice = DynamicCast<WifiNetDevice>(apDevice);
      Ptr<WifiPhy> phy = wifiNetDevice->GetPhy();

      if (phy) {
        phy->SetRxGain(interferenceRss);
      }

      Simulator::Schedule(Seconds(0.000001), [socket]() {socket->Close();});
  });

  Ptr<WifiNetDevice> wifiNetDevicePrimary = DynamicCast<WifiNetDevice>(staDevices.Get(0));
  Ptr<WifiPhy> phyPrimary = wifiNetDevicePrimary->GetPhy();

  if (phyPrimary) {
    phyPrimary->SetRxGain(primaryRss);
  }

  if (tracing) {
    wifiPhy.EnablePcapAll("wifi-interference");
  }

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}