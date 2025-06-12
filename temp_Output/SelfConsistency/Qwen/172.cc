#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/aodv-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/position-allocator.h"
#include "ns3/mobility-helper.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/udp-server.h"
#include "ns3/udp-client.h"
#include "ns3/on-off-helper.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/config.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AodvAdhocSimulation");

class UdpTrafficHelper : public Object
{
public:
  static TypeId GetTypeId(void)
  {
    static TypeId tid = TypeId("UdpTrafficHelper")
      .SetParent<Object>()
      .AddConstructor<UdpTrafficHelper>();
    return tid;
  }

  void Setup(Ptr<Node> sender, Ptr<Node> receiver, uint16_t port, double startTime)
  {
    Ipv4Address receiverAddr = receiver->GetObject<Ipv4>()->GetAddress(1,0).GetLocal();

    OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(receiverAddr, port));
    onoff.SetConstantRate(DataRate("1024bps"));
    onoff.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer apps = onoff.Install(sender);
    apps.Start(Seconds(startTime));
    apps.Stop(Seconds(30.0));

    PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    apps = sink.Install(receiver);
    apps.Start(Seconds(0.0));
    m_sink = DynamicCast<PacketSink>(apps.Get(0));
  }

  Ptr<PacketSink> GetSink() const { return m_sink; }

private:
  Ptr<PacketSink> m_sink;
};

int main(int argc, char *argv[])
{
  CommandLine cmd(__FILE__);
  cmd.Parse(argc, argv);

  RngSeedManager::SetSeed(12345);
  RngSeedManager::SetRun(7);

  NodeContainer nodes;
  nodes.Create(10);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
  wifiPhy.SetChannel(wifiChannel.Create());

  WifiMacHelper wifiMac;
  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211a);
  wifi.SetRemoteStationManager("ns3::ArfWifiManager");

  wifiMac.SetType("ns3::AdhocWifiMac");
  NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(100.0),
                                "DeltaY", DoubleValue(100.0),
                                "GridWidth", UintegerValue(5),
                                "LayoutType", StringValue("RowFirst"));

  mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                            "Bounds", RectangleValue(Rectangle(0, 500, 0, 500)));
  mobility.Install(nodes);

  AodvHelper aodv;
  Ipv4ListRoutingHelper listRh;
  listRh.Add(aodv, 100);

  InternetStackHelper internet;
  internet.SetRoutingHelper(listRh);
  internet.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.0.0.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  Config::SetDefault("ns3::Ipv4GlobalRouting::PerflowEcmp", BooleanValue(true));

  std::vector<Ptr<UdpTrafficHelper>> trafficHelpers;
  std::vector<Ptr<PacketSink>> sinks;

  for (uint32_t i = 0; i < 10; ++i)
  {
    uint32_t src = rand() % 10;
    uint32_t dst = rand() % 10;
    while (src == dst)
    {
      dst = rand() % 10;
    }
    double start = Seconds(UniformRandomVariable().GetValue(1.0, 20.0));

    Ptr<UdpTrafficHelper> helper = CreateObject<UdpTrafficHelper>();
    helper->Setup(nodes.Get(src), nodes.Get(dst), 9, start);
    trafficHelpers.push_back(helper);
    sinks.push_back(helper->GetSink());
  }

  wifiPhy.EnablePcapAll("aodv_adhoc_simulation");

  Simulator::Schedule(Seconds(30.0), [](){
    double totalReceived = 0;
    double totalSent = 0;
    double totalDelay = 0;

    for (auto sink : sinks)
    {
      totalReceived += sink->GetTotalRx();
      totalSent += sink->GetTotalTx();
      if (sink->GetTotalRx() > 0)
      {
        totalDelay += sink->GetAverageRtt().ToSeconds() / sink->GetTotalRx();
      }
    }

    double pdr = (totalSent > 0) ? (totalReceived / totalSent) : 0.0;
    double avgDelay = (totalReceived > 0) ? (totalDelay / totalReceived) : 0.0;

    NS_LOG_UNCOND("Packet Delivery Ratio: " << pdr * 100 << "% (" << totalReceived << "/" << totalSent << ")");
    NS_LOG_UNCOND("Average End-to-End Delay: " << avgDelay << " seconds");
  });

  Simulator::Stop(Seconds(30.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}