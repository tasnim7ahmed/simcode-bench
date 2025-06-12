#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/olsr-helper.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MixedWiredWirelessTopology");

class NodeContainerVector : public std::vector<NodeContainer>
{
};

struct SimulationParameters
{
  uint32_t backboneRouters;
  uint32_t lanNodesPerRouter;
  uint32_t wirelessStasPerAp;
  double simulationTime;
  bool tracing;
  bool enableMobility;
  bool animate;
};

SimulationParameters g_simParams;

void EnableCourseChangeLogging()
{
  AsciiTraceHelper ascii;
  Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream("course-changes.log");
  Config::Connect("/NodeList/*/$ns3::MobilityModel/CourseChange", MakeBoundCallback(&CourseChangeCallback, stream));
}

void CourseChangeCallback(Ptr<OutputStreamWrapper> stream, Ptr<const MobilityModel> model)
{
  Vector position = model->GetPosition();
  *stream->GetStream() << Simulator::Now().GetSeconds() << " "
                       << model->GetObject<Node>()->GetId() << " "
                       << position.x << " " << position.y << " " << position.z << std::endl;
}

int main(int argc, char *argv[])
{
  g_simParams.backboneRouters = 3;
  g_simParams.lanNodesPerRouter = 2;
  g_simParams.wirelessStasPerAp = 2;
  g_simParams.simulationTime = 10.0;
  g_simParams.tracing = false;
  g_simParams.enableMobility = true;
  g_simParams.animate = false;

  CommandLine cmd(__FILE__);
  cmd.AddValue("backboneRouters", "Number of backbone routers", g_simParams.backboneRouters);
  cmd.AddValue("lanNodesPerRouter", "Number of LAN nodes per router", g_simParams.lanNodesPerRouter);
  cmd.AddValue("wirelessStasPerAp", "Number of wireless stations per AP", g_simParams.wirelessStasPerAp);
  cmd.AddValue("simulationTime", "Total simulation time (seconds)", g_simParams.simulationTime);
  cmd.AddValue("tracing", "Enable packet tracing", g_simParams.tracing);
  cmd.AddValue("enableMobility", "Enable mobility models", g_simParams.enableMobility);
  cmd.AddValue("animate", "Enable NetAnim XML trace output", g_simParams.animate);
  cmd.Parse(argc, argv);

  if (g_simParams.backboneRouters < 2)
    {
      NS_FATAL_ERROR("Need at least two backbone routers for a connected topology.");
    }

  Time::SetResolution(Time::NS);
  LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
  LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

  // Create all backbone routers
  NodeContainer backboneNodes;
  backboneNodes.Create(g_simParams.backboneRouters);

  // Containers for LANs and infrastructure networks
  NodeContainerVector lanHosts(g_simParams.backboneRouters);
  NodeContainerVector infraAps(g_simParams.backboneRouters);
  NodeContainerVector infraStas(g_simParams.backboneRouters);

  PointToPointHelper p2pHelper;
  p2pHelper.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  p2pHelper.SetChannelAttribute("Delay", StringValue("2ms"));

  InternetStackHelper internet;
  OlsrHelper olsr;
  internet.SetRoutingHelper(olsr);
  internet.Install(backboneNodes);

  WifiMacHelper wifiMac;
  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211a);
  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue("OfdmRate54Mbps"), "ControlMode", StringValue("OfdmRate54Mbps"));

  YansWifiPhyHelper wifiPhy;
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
  wifiPhy.SetChannel(wifiChannel.Create());

  // Infrastructure network setup
  Ssid ssid = Ssid("mixed-network");

  for (uint32_t i = 0; i < g_simParams.backboneRouters; ++i)
    {
      // Create LAN hosts
      lanHosts[i].Create(g_simParams.lanNodesPerRouter);
      internet.Install(lanHosts[i]);

      // Create AP and STAs for infrastructure network
      infraAps[i].Create(1);
      infraStas[i].Create(g_simParams.wirelessStasPerAp);
      internet.Install(infraStas[i]);

      // Install WiFi on AP and STAs
      wifiMac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid), "BeaconInterval", TimeValue(Seconds(2.5)));
      NetDeviceContainer apDevice = wifi.Install(wifiPhy, wifiMac, infraAps[i]);
      wifiMac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));
      NetDeviceContainer staDevices = wifi.Install(wifiPhy, wifiMac, infraStas[i]);

      // Connect AP to its backbone router via point-to-point link
      NetDeviceContainer backboneLink = p2pHelper.Install(backboneNodes.Get(i), infraAps[i].Get(0));

      // Connect LAN nodes to their backbone router via point-to-point links
      for (uint32_t j = 0; j < lanHosts[i].GetN(); ++j)
        {
          p2pHelper.Install(backboneNodes.Get(i), lanHosts[i].Get(j));
        }
    }

  // Ad hoc WiFi network among backbone routers
  wifiMac.SetType("ns3::AdhocWifiMac");
  wifi.Install(wifiPhy, wifiMac, backboneNodes);

  // Assign IP addresses
  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.0.0", "255.255.0.0");

  for (uint32_t i = 0; i < g_simParams.backboneRouters; ++i)
    {
      for (uint32_t j = 0; j < lanHosts[i].GetN(); ++j)
        {
          ipv4.Assign(p2pHelper.GetNetDevices());
          ipv4.NewNetwork();
        }
      for (uint32_t j = 0; j < infraAps[i].GetN(); ++j)
        {
          ipv4.Assign(p2pHelper.GetNetDevices());
          ipv4.NewNetwork();
        }
    }

  ipv4.Assign(wifiPhy.Install(backboneNodes));
  ipv4.Assign(wifiPhy.Install(infraAps[0]));

  for (uint32_t i = 0; i < g_simParams.backboneRouters; ++i)
    {
      for (uint32_t j = 0; j < infraStas[i].GetN(); ++j)
        {
          ipv4.Assign(infraStas[i].Get(j)->GetDevice(0));
        }
    }

  // Setup mobility
  if (g_simParams.enableMobility)
    {
      MobilityHelper mobility;
      mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                    "MinX", DoubleValue(0.0),
                                    "MinY", DoubleValue(0.0),
                                    "DeltaX", DoubleValue(5.0),
                                    "DeltaY", DoubleValue(5.0),
                                    "GridWidth", UintegerValue(10),
                                    "LayoutType", StringValue("RowFirst"));
      mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                                "Bounds", RectangleValue(Rectangle(-50, 50, -50, 50)));
      mobility.Install(backboneNodes);

      for (auto &stas : infraStas)
        {
          mobility.Install(stas);
        }

      if (g_simParams.animate)
        {
          AnimationInterface anim("mixed-topology.xml");
          for (uint32_t i = 0; i < backboneNodes.GetN(); ++i)
            anim.UpdateNodeDescription(backboneNodes.Get(i), "BackboneRouter");
          for (auto &hosts : lanHosts)
            for (uint32_t i = 0; i < hosts.GetN(); ++i)
              anim.UpdateNodeDescription(hosts.Get(i), "LANHost");
          for (auto &aps : infraAps)
            for (uint32_t i = 0; i < aps.GetN(); ++i)
              anim.UpdateNodeDescription(aps.Get(i), "WiFiAP");
          for (auto &stas : infraStas)
            for (uint32_t i = 0; i < stas.GetN(); ++i)
              anim.UpdateNodeDescription(stas.Get(i), "WiFiSTA");
        }
    }

  // Enable course change callbacks
  EnableCourseChangeLogging();

  // Set up UDP flow from first LAN node to last wireless STA
  uint32_t destNodeId = infraStas[g_simParams.backboneRouters - 1].Get(0)->GetId();
  Ipv4Address destIp = infraStas[g_simParams.backboneRouters - 1].Get(0)->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal();

  UdpServerHelper server(9);
  ApplicationContainer serverApp = server.Install(infraStas[g_simParams.backboneRouters - 1].Get(0));
  serverApp.Start(Seconds(1.0));
  serverApp.Stop(Seconds(g_simParams.simulationTime));

  UdpClientHelper client(destIp, 9);
  client.SetAttribute("MaxPackets", UintegerValue(1000000));
  client.SetAttribute("Interval", TimeValue(Seconds(0.01)));
  client.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApp = client.Install(lanHosts[0].Get(0));
  clientApp.Start(Seconds(2.0));
  clientApp.Stop(Seconds(g_simParams.simulationTime));

  // Tracing
  if (g_simParams.tracing)
    {
      AsciiTraceHelper ascii;
      wifiPhy.EnableAsciiAll(ascii.CreateFileStream("wifi-trace.tr"));
      p2pHelper.EnablePcapAll("p2p-trace");
    }

  Simulator::Stop(Seconds(g_simParams.simulationTime));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}