#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/config-store-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WirelessInterferenceSimulation");

class PacketSinkHelper : public ApplicationHelper
{
public:
  PacketSinkHelper()
    : ApplicationHelper("ns3::PacketSink")
  {
    m_protocol = TypeId::LookupByName("ns3::UdpSocketFactory");
    SetAttribute("Protocol", PointerValue(CreateObject<UdpSocketFactory>()));
  }

  ApplicationContainer Install(Ptr<Node> node) const
  {
    return ApplicationHelper::Install(node);
  }

  ApplicationContainer Install(NodeContainer nodes) const
  {
    ApplicationContainer apps;
    for (NodeContainer::Iterator i = nodes.Begin(); i != nodes.End(); ++i)
      {
        apps.Add(Install(*i));
      }
    return apps;
  }
};

int main(int argc, char *argv[])
{
  double primaryRss = -80;       // dBm
  double interfererRss = -70;    // dBm
  double timeOffset = 0.0001;    // seconds
  uint32_t packetSizePrimary = 1000;
  uint32_t packetSizeInterferer = 500;
  bool verbose = false;
  std::string pcapFile = "wireless-interference.pcap";

  CommandLine cmd(__FILE__);
  cmd.AddValue("primaryRss", "Received Signal Strength of Primary Transmission (dBm)", primaryRss);
  cmd.AddValue("interfererRss", "Received Signal Strength of Interfering Transmission (dBm)", interfererRss);
  cmd.AddValue("timeOffset", "Time offset between primary and interferer (seconds)", timeOffset);
  cmd.AddValue("packetSizePrimary", "Packet size of primary transmission (bytes)", packetSizePrimary);
  cmd.AddValue("packetSizeInterferer", "Packet size of interfering transmission (bytes)", packetSizeInterferer);
  cmd.AddValue("verbose", "Enable verbose logging output", verbose);
  cmd.AddValue("pcapFile", "PCAP file name to write", pcapFile);
  cmd.Parse(argc, argv);

  if (verbose)
    {
      LogComponentEnable("WifiPhy", LOG_LEVEL_ALL);
      LogComponentEnable("WifiMac", LOG_LEVEL_ALL);
    }

  // Create three nodes: transmitter, receiver, interferer
  NodeContainer nodes;
  nodes.Create(3);

  // Setup physical layer
  YansWifiPhyHelper phy;
  phy.Set("RxGain", DoubleValue(0));
  phy.Set("TxGain", DoubleValue(0));

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  Ptr<YansWifiChannel> wifiChannel = channel.Create();
  phy.SetChannel(wifiChannel);

  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211b);
  wifi.SetRemoteStationManager("ns3::ArfWifiManager");

  WifiMacHelper mac;
  Ssid ssid = Ssid("wireless-network");

  NetDeviceContainer devices;

  // Transmitter (node 0)
  mac.SetType("ns3::AdhocWifiMac",
              "Ssid", SsidValue(ssid),
              "ActiveProbing", BooleanValue(false));
  devices.Add(wifi.Install(phy, mac, nodes.Get(0)));

  // Receiver (node 1)
  mac.SetType("ns3::AdhocWifiMac",
              "Ssid", SsidValue(ssid),
              "ActiveProbing", BooleanValue(false));
  devices.Add(wifi.Install(phy, mac, nodes.Get(1)));

  // Interferer (node 2)
  mac.SetType("ns3::AdhocWifiMac",
              "Ssid", SsidValue(ssid),
              "ActiveProbing", BooleanValue(false));
  devices.Add(wifi.Install(phy, mac, nodes.Get(2)));

  // Assign fixed positions
  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(10.0),
                                "DeltaY", DoubleValue(0.0),
                                "GridWidth", UintegerValue(3),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(nodes);

  // Internet stack
  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.0.0.0", "255.255.255.0");

  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  // Applications
  uint16_t port = 9;

  // Receiver application on node 1
  PacketSinkHelper sink;
  ApplicationContainer sinkApp = sink.Install(nodes.Get(1));
  sinkApp.Start(Seconds(0.0));
  sinkApp.Stop(Seconds(1.0));

  // Primary sender on node 0
  OnOffHelper onoff1("ns3::UdpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), port));
  onoff1.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  onoff1.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  onoff1.SetAttribute("DataRate", DataRateValue(DataRate("1kbps")));
  onoff1.SetAttribute("PacketSize", UintegerValue(packetSizePrimary));
  ApplicationContainer app1 = onoff1.Install(nodes.Get(0));
  app1.Start(Seconds(0.0));
  app1.Stop(Seconds(1.0));

  // Interferer on node 2
  OnOffHelper onoff2("ns3::UdpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), port));
  onoff2.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  onoff2.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  onoff2.SetAttribute("DataRate", DataRateValue(DataRate("1kbps")));
  onoff2.SetAttribute("PacketSize", UintegerValue(packetSizeInterferer));
  ApplicationContainer app2 = onoff2.Install(nodes.Get(2));
  app2.Start(Seconds(timeOffset));
  app2.Stop(Seconds(1.0));

  // Configure transmit power for each node
  Ptr<WifiPhy> phy0 = nodes.Get(0)->GetDevice(0)->GetObject<WifiNetDevice>()->GetPhy();
  Ptr<WifiPhy> phy1 = nodes.Get(1)->GetDevice(0)->GetObject<WifiNetDevice>()->GetPhy();
  Ptr<WifiPhy> phy2 = nodes.Get(2)->GetDevice(0)->GetObject<WifiNetDevice>()->GetPhy();

  // Set received signal strength by adjusting TX power accordingly
  // This is a simplified approach assuming free-space propagation and fixed distance
  double distance0 = 10.0; // meters from node 0 to node 1
  double distance2 = 10.0; // meters from node 2 to node 1

  double txPower0 = primaryRss + (10 * 2 * log10(distance0 / 1.0)); // Free space path loss model n=2
  double txPower2 = interfererRss + (10 * 2 * log10(distance2 / 1.0));

  NS_LOG_INFO("Setting TX power of primary to " << txPower0 << " dBm");
  NS_LOG_INFO("Setting TX power of interferer to " << txPower2 << " dBm");

  phy0->SetTxPowerStart(txPower0);
  phy0->SetTxPowerEnd(txPower0);
  phy2->SetTxPowerStart(txPower2);
  phy2->SetTxPowerEnd(txPower2);

  // Enable PCAP tracing
  phy.EnablePcap(pcapFile, devices.Get(1), true, true);

  Simulator::Stop(Seconds(1.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}