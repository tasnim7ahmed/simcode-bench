#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/olsr-helper.h"
#include "ns3/csma-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-list-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MixedWiredWirelessTopology");

class BackboneRouter
{
public:
  NodeContainer backboneNodes;
  NetDeviceContainer wifiApDevices;
  NetDeviceContainer wifiStaDevices;
  NetDeviceContainer csmaDevices;
  Ipv4InterfaceContainer backboneInterfaces;
  Ipv4InterfaceContainer apInterfaces;
  Ipv4InterfaceContainer staInterfaces;
  Ipv4InterfaceContainer csmaInterfaces;
};

int main(int argc, char *argv[])
{
  uint32_t numBackboneRouters = 3;
  uint32_t numLanNodesPerRouter = 2;
  uint32_t numWirelessStationsPerAp = 2;
  double simulationTime = 10.0;
  bool enableTracing = false;
  bool enableMobility = true;
  bool enableCourseChangeCallback = false;

  CommandLine cmd(__FILE__);
  cmd.AddValue("numBackboneRouters", "Number of backbone routers", numBackboneRouters);
  cmd.AddValue("numLanNodesPerRouter", "Number of LAN nodes per router", numLanNodesPerRouter);
  cmd.AddValue("numWirelessStationsPerAp", "Number of wireless stations per AP", numWirelessStationsPerAp);
  cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue("enableTracing", "Enable PCAP tracing", enableTracing);
  cmd.AddValue("enableMobility", "Enable mobility models", enableMobility);
  cmd.AddValue("enableCourseChangeCallback", "Enable course change callbacks", enableCourseChangeCallback);
  cmd.Parse(argc, argv);

  Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue("ns3::TcpNewReno"));

  NodeContainer backboneNodes;
  backboneNodes.Create(numBackboneRouters);

  NodeContainer lanNodes;
  lanNodes.Create(numBackboneRouters * numLanNodesPerRouter);

  NodeContainer wirelessStas;
  wirelessStas.Create(numBackboneRouters * numWirelessStationsPerAp);

  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper wifiPhy;
  wifiPhy.SetChannel(wifiChannel.Create());

  WifiHelper wifi;
  wifi.SetRemoteStationManager("ns3::ArfWifiManager");

  WifiMacHelper wifiMac;
  wifiMac.SetType("ns3::AdhocWifiMac");

  NetDeviceContainer backboneWifiDevices;
  backboneWifiDevices = wifi.Install(wifiPhy, wifiMac, backboneNodes);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer p2pDevices;
  for (uint32_t i = 0; i < numBackboneRouters - 1; ++i)
    {
      p2pDevices.Add(p2p.Install(backboneNodes.Get(i), backboneNodes.Get(i + 1)));
    }

  InternetStackHelper internet;
  OlsrHelper olsr;
  Ipv4ListRoutingHelper list;
  list.Add(olsr, 10);

  Ipv4StaticRoutingHelper staticRouting;
  list.Add(staticRouting, 5);

  internet.SetRoutingHelper(list);
  internet.Install(backboneNodes);
  internet.Install(lanNodes);
  internet.Install(wirelessStas);

  CsmaHelper csma;
  csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
  csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

  std::vector<NetDeviceContainer> csmaDevs;
  std::vector<Ipv4InterfaceContainer> csmaIfs;

  for (uint32_t i = 0; i < numBackboneRouters; ++i)
    {
      NodeContainer localLan;
      for (uint32_t j = 0; j < numLanNodesPerRouter; ++j)
        {
          localLan.Add(lanNodes.Get(i * numLanNodesPerRouter + j));
        }

      NetDeviceContainer devs = csma.Install(NodeContainer(backboneNodes.Get(i), localLan));
      csmaDevs.push_back(devs);

      Ipv4AddressHelper ip;
      ip.SetBase("10.1." + std::to_string(i) + ".0", "255.255.255.0");
      Ipv4InterfaceContainer ifc = ip.Assign(devs);
      csmaIfs.push_back(ifc);
    }

  wifiMac.SetType("ns3::StaWifiMac",
                  "Ssid", SsidValue(Ssid("infrastructure")),
                  "ActiveProbing", BooleanValue(false));

  NetDeviceContainer staDevices[numBackboneRouters];
  for (uint32_t i = 0; i < numBackboneRouters; ++i)
    {
      NodeContainer stas;
      for (uint32_t j = 0; j < numWirelessStationsPerAp; ++j)
        {
          stas.Add(wirelessStas.Get(i * numWirelessStationsPerAp + j));
        }
      staDevices[i] = wifi.Install(wifiPhy, wifiMac, stas);
    }

  wifiMac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(Ssid("infrastructure")));

  NetDeviceContainer apDevices[numBackboneRouters];
  for (uint32_t i = 0; i < numBackboneRouters; ++i)
    {
      apDevices[i] = wifi.Install(wifiPhy, wifiMac, backboneNodes.Get(i));
    }

  std::vector<Ipv4InterfaceContainer> apIfs;
  std::vector<Ipv4InterfaceContainer> staIfs;

  for (uint32_t i = 0; i < numBackboneRouters; ++i)
    {
      Ipv4AddressHelper ip;
      ip.SetBase("192.168." + std::to_string(i) + ".0", "255.255.255.0");
      apIfs.push_back(ip.Assign(apDevices[i]));
      staIfs.push_back(ip.Assign(staDevices[i]));
    }

  Ipv4Address sinkAddress = staIfs.back().GetAddress(0);
  uint16_t port = 9;

  OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(sinkAddress, port));
  onoff.SetConstantRate(DataRate("512kbps"));
  onoff.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer apps = onoff.Install(lanNodes.Get(0));
  apps.Start(Seconds(1.0));
  apps.Stop(Seconds(simulationTime));

  PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
  apps = sink.Install(wirelessStas.Get((numBackboneRouters - 1) * numWirelessStationsPerAp));
  apps.Start(Seconds(0.0));
  apps.Stop(Seconds(simulationTime));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  if (enableMobility)
    {
      MobilityHelper mobility;
      mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                    "MinX", DoubleValue(0.0),
                                    "MinY", DoubleValue(0.0),
                                    "DeltaX", DoubleValue(10.0),
                                    "DeltaY", DoubleValue(10.0),
                                    "GridWidth", UintegerValue(10),
                                    "LayoutType", StringValue("RowFirst"));

      if (enableCourseChangeCallback)
        {
          mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                                    "Bounds", RectangleValue(Rectangle(-50, 50, -50, 50)));
        }
      else
        {
          mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
        }

      mobility.Install(backboneNodes);
      mobility.Install(wirelessStas);
    }

  if (enableTracing)
    {
      AsciiTraceHelper ascii;
      Simulator::CreateAsciiTraceFile("mixed_topology.tr");
      wifiPhy.EnableAsciiAll(ascii.CreateFileStream("mixed_topology.tr"));
      wifiPhy.EnablePcapAll("mixed_topology");
    }

  Simulator::Stop(Seconds(simulationTime));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}