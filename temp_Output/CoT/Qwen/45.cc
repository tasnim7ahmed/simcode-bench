#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WirelessInterferenceSimulation");

class WirelessInterferenceExperiment
{
public:
  WirelessInterferenceExperiment();
  void Setup(int64_t seed);
  void Run();
  void Report();

private:
  NodeContainer m_nodes;
  YansWifiChannelHelper m_wifiChannel;
  WifiHelper m_wifi;
  WifiMacHelper m_wifiMac;
  InternetStackHelper m_internet;
  Ipv4AddressHelper m_address;
  double m_primaryRss; // dBm
  double m_interfererRss; // dBm
  Time m_interferenceOffset;
  uint32_t m_primaryPacketSize;
  uint32_t m_interfererPacketSize;
  bool m_verboseLogging;
  bool m_pcapTracing;
};

WirelessInterferenceExperiment::WirelessInterferenceExperiment()
  : m_primaryRss(-80.0),
    m_interfererRss(-70.0),
    m_interferenceOffset(Seconds(0.1)),
    m_primaryPacketSize(1024),
    m_interfererPacketSize(512),
    m_verboseLogging(false),
    m_pcapTracing(true)
{
  CommandLine cmd(__FILE__);
  cmd.AddValue("primaryRss", "RSS of the primary transmission in dBm", m_primaryRss);
  cmd.AddValue("interfererRss", "RSS of the interferer transmission in dBm", m_interfererRss);
  cmd.AddValue("interferenceOffset", "Time offset between primary and interference [s]", m_interferenceOffset);
  cmd.AddValue("primaryPacketSize", "Size of packets sent by primary node", m_primaryPacketSize);
  cmd.AddValue("interfererPacketSize", "Size of packets sent by interferer node", m_interfererPacketSize);
  cmd.AddValue("verbose", "Enable verbose logging output", m_verboseLogging);
  cmd.AddValue("pcap", "Enable pcap tracing", m_pcapTracing);
  cmd.Parse(Simulator::GetArgumentc(), Simulator::GetArguments());
}

void WirelessInterferenceExperiment::Setup(int64_t seed)
{
  RngSeedManager::SetSeed(seed);
  RngSeedManager::SetRun(1);

  m_nodes.Create(3); // 0: transmitter, 1: receiver, 2: interferer

  m_wifiChannel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper wifiPhy;
  wifiPhy.SetChannel(m_wifiChannel.Create());
  wifiPhy.Set("TxPowerStart", DoubleValue(16.0));
  wifiPhy.Set("TxPowerEnd", DoubleValue(16.0));
  wifiPhy.Set("RxGain", DoubleValue(0.0));

  m_wifi = WifiHelper();
  m_wifi.SetStandard(WIFI_STANDARD_80211b);
  if (m_verboseLogging)
    {
      m_wifi.EnableLogComponents();
    }

  m_wifiMac = WifiMacHelper();
  Ssid ssid = Ssid("wifi-interference-experiment");
  m_wifiMac.SetType("ns3::AdhocWifiMac");

  NetDeviceContainer devices = m_wifi.Install(wifiPhy, m_wifiMac, m_nodes);

  if (m_pcapTracing)
    {
      wifiPhy.EnablePcapAll("wireless-interference", false);
    }

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(5.0),
                                "DeltaY", DoubleValue(0.0),
                                "GridWidth", UintegerValue(3),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(m_nodes);

  m_internet.Install(m_nodes);
  m_address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = m_address.Assign(devices);

  TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
  InetSocketAddress destAddr(interfaces.GetAddress(1), 9); // Receiver's port 9 for UDP echo

  // Primary packet sink on node 1
  PacketSinkHelper sinkHelper(tid, InetSocketAddress(Ipv4Address::GetAny(), 9));
  ApplicationContainer sinkApp = sinkHelper.Install(m_nodes.Get(1));
  sinkApp.Start(Seconds(0.0));
  sinkApp.Stop(Seconds(10.0));

  // Primary source on node 0
  OnOffHelper onoff(tid, destAddr);
  onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  onoff.SetAttribute("DataRate", DataRateValue(DataRate("1Mbps")));
  onoff.SetAttribute("PacketSize", UintegerValue(m_primaryPacketSize));
  ApplicationContainer sourceApp = onoff.Install(m_nodes.Get(0));
  sourceApp.Start(Seconds(0.0));
  sourceApp.Stop(Seconds(10.0));

  // Interferer on node 2, sends at t = m_interferenceOffset
  OnOffHelper interferer(tid, destAddr);
  interferer.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  interferer.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  interferer.SetAttribute("DataRate", DataRateValue(DataRate("1Mbps")));
  interferer.SetAttribute("PacketSize", UintegerValue(m_interfererPacketSize));
  ApplicationContainer interfererApp = interferer.Install(m_nodes.Get(2));
  interfererApp.Start(m_interferenceOffset);
  interfererApp.Stop(Seconds(10.0));

  // Disable carrier sense on interferer
  Ptr<WifiNetDevice> interfererDevice = DynamicCast<WifiNetDevice>(m_nodes.Get(2)->GetDevice(0));
  if (interfererDevice)
    {
      interfererDevice->GetMac()->SetAttribute("Csmaca_Enable", BooleanValue(false));
    }

  // Set RSS for primary and interferer using propagation loss
  Ptr<YansWifiChannel> channel = DynamicCast<YansWifiChannel>(wifiPhy.GetChannel());
  if (channel)
    {
      Ptr<PropagationLossModel> lossModel = channel->GetPropagationLoss();
      if (!lossModel)
        {
          lossModel = CreateObject<RangePropagationLossModel>();
          channel->AddPropagationLoss(lossModel);
        }

      // Set distance to achieve desired RSS
      // RSSI = TxPower - PathLoss
      // For fixed TxPower of 16dBm, compute distance to get target RSS
      // Assuming FreeSpace model here
      Ptr<PropagationDelayModel> delayModel = channel->GetPropagationDelay();
      channel = CreateObject<YansWifiChannel>();
      channel->SetPropagationLoss(lossModel);
      channel->SetPropagationDelay(delayModel);

      // Recreate phy with new channel
      wifiPhy = YansWifiPhyHelper();
      wifiPhy.SetChannel(channel);
      wifiPhy.Set("TxPowerStart", DoubleValue(16.0));
      wifiPhy.Set("TxPowerEnd", DoubleValue(16.0));
      wifiPhy.Set("RxGain", DoubleValue(0.0));
      devices = m_wifi.Install(wifiPhy, m_wifiMac, m_nodes);

      // Calculate distance for each node to achieve desired RSS
      // Using FreeSpace formula: PL(d) = 32.4 + 20*log10(f) + 20*log10(d) where f is in MHz, d in km
      double freqMHz = 2407.0; // Channel 1 in 802.11b
      auto calculateDistance = [freqMHz](double rssiDbm)
      {
        double pl = 16.0 - rssiDbm; // TX power is 16 dBm
        return pow(10, (pl - 32.4 - 20 * log10(freqMHz)) / 20.0) / 1000.0; // in km
      };

      double distancePrimary = calculateDistance(m_primaryRss);
      double distanceInterferer = calculateDistance(m_interfererRss);

      // Set positions accordingly
      // Node 0 (tx): origin
      // Node 1 (rx): distancePrimary away along x-axis
      // Node 2 (interferer): distanceInterferer away along x-axis
      mobility = MobilityHelper();
      mobility.SetPositionAllocator("ns3::ListPositionAllocator");
      Ptr<ListPositionAllocator> positionAlloc = mobility.GetPositionAllocator()->GetObject<ListPositionAllocator>();
      positionAlloc->Add(Vector(0.0, 0.0, 0.0)); // tx
      positionAlloc->Add(Vector(distancePrimary, 0.0, 0.0)); // rx
      positionAlloc->Add(Vector(distanceInterferer, 0.0, 0.0)); // interferer

      mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
      mobility.Install(m_nodes);
    }
}

void WirelessInterferenceExperiment::Run()
{
  Simulator::Run();
}

void WirelessInterferenceExperiment::Report()
{
  Simulator::Destroy();
}

int main(int argc, char* argv[])
{
  WirelessInterferenceExperiment experiment;
  experiment.Setup(12345);
  experiment.Run();
  experiment.Report();
  return 0;
}