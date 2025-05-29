#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/olsr-helper.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/ssid.h"
#include "ns3/wifi-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/csma-helper.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-helper.h"

using namespace ns3;

void CourseChange(std::string context, Ptr<const MobilityModel> model)
{
  Vector position = model->GetPosition();
  NS_LOG_UNCOND(Simulator::Now().GetSeconds() << "s: " << context << " moved to: x=" << position.x << ", y=" << position.y);
}

int main(int argc, char *argv[])
{
  uint32_t nBackbone = 3;
  uint32_t nLan = 3;
  uint32_t nSta = 2;
  bool enableTracing = false;
  bool enableCourseChange = false;
  double simTime = 20.0;

  CommandLine cmd;
  cmd.AddValue("nBackbone", "Number of backbone routers", nBackbone);
  cmd.AddValue("nLan", "Number of nodes per LAN", nLan);
  cmd.AddValue("nSta", "Number of stations per WiFi infrastructure", nSta);
  cmd.AddValue("enableTracing", "Enable PCAP/Ascii Traces", enableTracing);
  cmd.AddValue("enableCourseChange", "Enable course change callback", enableCourseChange);
  cmd.AddValue("simTime", "Simulation time [s]", simTime);
  cmd.Parse(argc, argv);

  LogComponentEnable("UdpClient", LOG_LEVEL_INFO);

  NodeContainer backboneRouters;
  backboneRouters.Create(nBackbone);

  std::vector<NodeContainer> lanNodes(nBackbone);
  std::vector<NodeContainer> apNodes(nBackbone);
  std::vector<NodeContainer> staNodes(nBackbone);

  for (uint32_t i = 0; i < nBackbone; ++i)
  {
    // One backbone router, nLan-1 new nodes
    lanNodes[i].Add(backboneRouters.Get(i));
    NodeContainer extraLanNodes;
    extraLanNodes.Create(nLan - 1);
    lanNodes[i].Add(extraLanNodes);

    apNodes[i].Add(backboneRouters.Get(i));

    NodeContainer stas;
    stas.Create(nSta);
    staNodes[i].Add(stas);
  }

  // Wired LAN setup
  CsmaHelper csmaLan;
  csmaLan.SetChannelAttribute("DataRate", StringValue("100Mbps"));
  csmaLan.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

  std::vector<NetDeviceContainer> lanDevices(nBackbone);

  for (uint32_t i = 0; i < nBackbone; ++i)
    lanDevices[i] = csmaLan.Install(lanNodes[i]);

  // WiFi Infrastructure setup
  YansWifiChannelHelper infraChannel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper infraPhy = YansWifiPhyHelper::Default();
  infraPhy.SetChannel(infraChannel.Create());
  WifiHelper infraWifi;
  infraWifi.SetStandard(WIFI_STANDARD_80211b);
  WifiMacHelper infraMac;

  std::vector<NetDeviceContainer> apDevices(nBackbone), staDevices(nBackbone);

  for (uint32_t i = 0; i < nBackbone; ++i)
  {
    Ssid ssid = Ssid("ssid-infra-" + std::to_string(i));
    infraMac.SetType("ns3::StaWifiMac",
                     "Ssid", SsidValue(ssid),
                     "ActiveProbing", BooleanValue(false));
    staDevices[i] = infraWifi.Install(infraPhy, infraMac, staNodes[i]);
    infraMac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    apDevices[i] = infraWifi.Install(infraPhy, infraMac, apNodes[i]);
  }

  // Backbone Ad-hoc WiFi setup
  YansWifiChannelHelper adhocChannel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper adhocPhy = YansWifiPhyHelper::Default();
  adhocPhy.SetChannel(adhocChannel.Create());
  WifiHelper adhocWifi;
  adhocWifi.SetStandard(WIFI_STANDARD_80211b);
  WifiMacHelper adhocMac;
  adhocMac.SetType("ns3::AdhocWifiMac");

  NetDeviceContainer backboneWifiDevices = adhocWifi.Install(adhocPhy, adhocMac, backboneRouters);

  // Install TCP/IP stack
  InternetStackHelper stack;
  OlsrHelper olsr;
  Ipv4ListRoutingHelper list;
  list.Add(olsr, 10);
  list.Add(Ipv4StaticRoutingHelper(), 0);

  for (uint32_t i = 0; i < nBackbone; ++i)
    stack.SetRoutingHelper(list);
  stack.Install(backboneRouters);

  for (uint32_t i = 0; i < nBackbone; ++i)
  {
    stack.SetRoutingHelper(Ipv4StaticRoutingHelper());
    stack.Install(lanNodes[i].GetN() > 1 ? NodeContainer(lanNodes[i].Begin() + 1, lanNodes[i].End()) : NodeContainer());
    stack.Install(staNodes[i]);
  }

  // Addressing
  std::vector<Ipv4InterfaceContainer> lanIfaces(nBackbone), staIfaces(nBackbone), apIfaces(nBackbone);
  Ipv4AddressHelper address;

  // 10.1.x.0/24 for LANs
  for (uint32_t i = 0; i < nBackbone; ++i)
  {
    std::ostringstream subnet;
    subnet << "10.1." << i << ".0";
    address.SetBase(subnet.str().c_str(), "255.255.255.0");
    lanIfaces[i] = address.Assign(lanDevices[i]);
  }

  // 10.2.x.0/24 for Infrastructure WiFi
  for (uint32_t i = 0; i < nBackbone; ++i)
  {
    std::ostringstream subnet;
    subnet << "10.2." << i << ".0";
    address.SetBase(subnet.str().c_str(), "255.255.255.0");
    staIfaces[i] = address.Assign(staDevices[i]);
    apIfaces[i] = address.Assign(apDevices[i]);
  }

  // 10.3.0.0/24 for Ad-hoc backbone mesh
  address.SetBase("10.3.0.0", "255.255.255.0");
  Ipv4InterfaceContainer backboneIfaces = address.Assign(backboneWifiDevices);

  // Mobility
  MobilityHelper mobBackbone, mobAP, mobSTA, mobLAN;

  // Backbone routers: random grid
  mobBackbone.SetMobilityModel("ns3::RandomWaypointMobilityModel",
                               "Speed", StringValue("ns3::UniformRandomVariable[Min=2.0|Max=5.0]"),
                               "Pause", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"),
                               "PositionAllocator", PointerValue(CreateObjectWithAttributes<RandomRectanglePositionAllocator>(
                                   "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=120.0]"),
                                   "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=120.0]"))));
  mobBackbone.Install(backboneRouters);

  // LANs: static, near each backbone router
  for (uint32_t i = 0; i < nBackbone; ++i)
  {
    mobLAN.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobLAN.SetPositionAllocator("ns3::GridPositionAllocator",
                               "MinX", DoubleValue(10.0 + i * 40),
                               "MinY", DoubleValue(0.0),
                               "DeltaX", DoubleValue(3.0),
                               "DeltaY", DoubleValue(3.0),
                               "GridWidth", UintegerValue(nLan),
                               "LayoutType", StringValue("RowFirst"));
    mobLAN.Install(lanNodes[i]);
  }

  // APs: static, colocated with backbone
  for (uint32_t i = 0; i < nBackbone; ++i)
  {
    mobAP.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobAP.SetPositionAllocator("ns3::GridPositionAllocator",
                              "MinX", DoubleValue(10.0 + i * 40),
                              "MinY", DoubleValue(15.0),
                              "DeltaX", DoubleValue(0.0),
                              "DeltaY", DoubleValue(0.0),
                              "GridWidth", UintegerValue(1),
                              "LayoutType", StringValue("RowFirst"));
    mobAP.Install(apNodes[i]);
  }

  // STAs: random within area near APs
  for (uint32_t i = 0; i < nBackbone; ++i)
  {
    mobSTA.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                            "Bounds", RectangleValue(Rectangle(10.0 + i * 40, 30.0 + i * 40, 17.0, 37.0)));
    mobSTA.Install(staNodes[i]);
    if (enableCourseChange)
    {
      for (uint32_t j = 0; j < staNodes[i].GetN(); ++j)
      {
        Config::Connect("/NodeList/" + std::to_string(staNodes[i].Get(j)->GetId()) + "/$ns3::MobilityModel/CourseChange",
                        MakeBoundCallback(&CourseChange, ""));
      }
    }
  }

  if (enableCourseChange)
  {
    for (uint32_t i = 0; i < backboneRouters.GetN(); ++i)
    {
      Config::Connect("/NodeList/" + std::to_string(backboneRouters.Get(i)->GetId()) + "/$ns3::MobilityModel/CourseChange",
                      MakeBoundCallback(&CourseChange, "Backbone"));
    }
  }

  // UDP Flow: from first LAN node in LAN 0 to last STA in last infra network
  Ptr<Node> srcNode = lanNodes[0].Get(1);
  Ptr<Node> dstNode = staNodes[nBackbone - 1].Get(nSta - 1);
  Ipv4Address dstAddr = staIfaces[nBackbone - 1].GetAddress(nSta - 1);

  uint16_t udpPort = 4000;
  UdpServerHelper udpServer(udpPort);
  ApplicationContainer serverApp = udpServer.Install(dstNode);
  serverApp.Start(Seconds(1.0));
  serverApp.Stop(Seconds(simTime));

  UdpClientHelper udpClient(dstAddr, udpPort);
  udpClient.SetAttribute("MaxPackets", UintegerValue(10000));
  udpClient.SetAttribute("Interval", TimeValue(MilliSeconds(100)));
  udpClient.SetAttribute("PacketSize", UintegerValue(512));
  ApplicationContainer clientApp = udpClient.Install(srcNode);
  clientApp.Start(Seconds(2.0));
  clientApp.Stop(Seconds(simTime) - Seconds(1.0));

  // Tracing
  if (enableTracing)
  {
    csmaLan.EnablePcapAll("mixed-csma");
    YansWifiPhyHelper::Default().EnablePcapAll("mixed-wifi");
    AsciiTraceHelper ath;
    csmaLan.EnableAsciiAll(ath.CreateFileStream("mixed-csma.tr"));
  }

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Simulator::Stop(Seconds(simTime));
  Simulator::Run();
  monitor->SerializeToXmlFile("mixed-network-flowmon.xml", true, true);
  Simulator::Destroy();
  return 0;
}