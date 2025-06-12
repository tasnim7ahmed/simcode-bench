#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/aodv-helper.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/packet-sink.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AodvAdhocSimulation");

class UdpTrafficHelper {
public:
  ApplicationContainer Install(Ptr<Node> node, Ipv4Address address, uint16_t port) {
    UdpServerHelper server(port);
    ApplicationContainer apps = server.Install(node);
    apps.Start(Seconds(0.0));
    apps.Stop(Seconds(30.0));

    UdpClientHelper client(address, port);
    client.SetAttribute("MaxPackets", UintegerValue(1000));
    client.SetAttribute("Interval", TimeValue(Seconds(0.5)));
    client.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer app = client.Install(node);
    return app;
  }
};

int main(int argc, char *argv[]) {
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
  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                               "DataMode", StringValue("OfdmRate6Mbps"));

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
  Ipv4ListRoutingHelper listRouting;
  listRouting.Add(aodv, 100);

  InternetStackHelper internet;
  internet.SetRoutingHelper(listRouting);
  internet.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.0.0.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  PacketSinkHelper sink("ns3::UdpSocketFactory",
                        InetSocketAddress(Ipv4Address::GetAny(), 9));
  ApplicationContainer sinks = sink.Install(nodes);
  sinks.Start(Seconds(0.0));
  sinks.Stop(Seconds(30.0));

  UdpTrafficHelper udpHelper;
  ApplicationContainer apps;
  for (uint32_t i = 0; i < nodes.GetN(); ++i) {
    uint32_t j = i;
    while (j == i) {
      j = rand() % nodes.GetN();
    }
    Ptr<Node> sender = nodes.Get(i);
    Ipv4Address destAddr = interfaces.GetAddress(j, 0);
    Time startTime = Seconds(1 + (rand() % 20));
    ApplicationContainer clientApp = udpHelper.Install(sender, destAddr, 9);
    clientApp.Start(startTime);
    clientApp.Stop(Seconds(30.0));
  }

  Config::Connect("/NodeList/*/ApplicationList/*/$ns3::PacketSink/Rx",
                  MakeCallback([](Ptr<const Packet> p, const Address &) {
                    static uint32_t totalRx = 0;
                    totalRx++;
                    NS_LOG_DEBUG("Received packet " << totalRx);
                  }));

  wifiPhy.EnablePcapAll("aodv_adhoc_network");

  Simulator::Stop(Seconds(30.0));
  Simulator::Run();

  double pdr = 0.0;
  double delay = 0.0;
  uint32_t totalTx = 0;
  uint32_t totalRx = 0;
  Time totalDelay = Seconds(0);

  for (auto it = sinks.Begin(); it != sinks.End(); ++it) {
    Ptr<PacketSink> sink = DynamicCast<PacketSink>(*it);
    totalRx += sink->GetTotalRx();
    totalDelay += sink->GetTotalRxDelay();
  }

  for (auto it = apps.Begin(); it != apps.End(); ++it) {
    Ptr<UdpClient> client = DynamicCast<UdpClient>(*it);
    totalTx += client->GetSent();
  }

  if (totalTx > 0) {
    pdr = static_cast<double>(totalRx) / totalTx;
    delay = totalDelay.GetSeconds() / totalRx;
  }

  std::cout << "Packet Delivery Ratio: " << pdr << std::endl;
  std::cout << "Average End-to-End Delay: " << delay << " s" << std::endl;

  Simulator::Destroy();
  return 0;
}