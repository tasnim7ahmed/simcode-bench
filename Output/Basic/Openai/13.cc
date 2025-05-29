#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/ssid.h"
#include "ns3/olsr-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/csma-module.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/config-store-module.h"
#include "ns3/bridge-module.h"
#include <vector>
#include <sstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("MixedWiredWirelessTopology");

void
CourseChange (std::string path, Ptr<const MobilityModel> mobility)
{
  Vector pos = mobility->GetPosition ();
  NS_LOG_UNCOND (Simulator::Now ().GetSeconds () << "s: " << path << " moved to " << pos);
}

int main (int argc, char *argv[])
{
  uint32_t nBackbone = 3;
  uint32_t nLan = 2;
  uint32_t nWifi = 2;
  double simTime = 30.0;
  bool tracing = true;
  bool enableCourseChange = true;

  CommandLine cmd (__FILE__);
  cmd.AddValue ("nBackbone", "Number of backbone routers", nBackbone);
  cmd.AddValue ("nLan", "Number of nodes in each wired LAN", nLan);
  cmd.AddValue ("nWifi", "Number of WiFi STAs per BBR", nWifi);
  cmd.AddValue ("simTime", "Simulation time (s)", simTime);
  cmd.AddValue ("tracing", "Enable pcap tracing", tracing);
  cmd.AddValue ("enableCourseChange", "Enable mobility course change callbacks", enableCourseChange);
  cmd.Parse (argc, argv);

  // Containers for all nodes
  NodeContainer backboneRouters;
  backboneRouters.Create (nBackbone);

  std::vector<NodeContainer> lanNodes (nBackbone);
  std::vector<NodeContainer> wifiStaNodes (nBackbone);
  std::vector<Ptr<Node>> apNodes (nBackbone);

  // For assigning IP spaces
  std::vector<Ipv4InterfaceContainer> lanIfaces (nBackbone);
  std::vector<Ipv4InterfaceContainer> wifiStaIfaces (nBackbone);
  std::vector<Ipv4InterfaceContainer> wifiApIfaces (nBackbone);

  // 1. Install Internet stack and OLSR on all nodes, but OLSR only enabled on backbone and their APs
  InternetStackHelper internet;
  OlsrHelper olsr;
  Ipv4StaticRoutingHelper staticRouting;
  Ipv4ListRoutingHelper list;
  list.Add (olsr, 10);
  list.Add (staticRouting, 0);

  for (uint32_t i = 0; i < nBackbone; ++i)
    {
      // Create LAN nodes (excluding the backbone router itself)
      lanNodes[i].Create (nLan);

      // Create WiFi STA nodes
      wifiStaNodes[i].Create (nWifi);

      // AP node is the BBR itself (dual role)
      apNodes[i] = backboneRouters.Get(i);
    }

  // Install OLSR on all backbone routers (and their AP interface)
  internet.SetRoutingHelper (list);
  internet.Install (backboneRouters);

  // For LAN and WiFi STAs, no OLSR, just internet
  for (uint32_t i = 0; i < nBackbone; ++i)
    {
      internet.SetRoutingHelper (staticRouting);
      internet.Install (lanNodes[i]);
      internet.Install (wifiStaNodes[i]);
    }

  // 2. Create the backbone ad hoc WiFi network interconnecting all BBRs
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  phy.SetChannel (channel.Create ());
  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211b);
  WifiMacHelper mac;
  Ssid ssid = Ssid ("backbone-adhoc");
  mac.SetType ("ns3::AdhocWifiMac", "Ssid", SsidValue (ssid));
  NetDeviceContainer backboneWifiDevices = wifi.Install (phy, mac, backboneRouters);

  // 3. Attach a LAN to each BBR via CSMA (wired)
  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", DataRateValue (DataRate ("100Mbps")));
  csma.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (1)));

  std::vector<NetDeviceContainer> lanDevices (nBackbone);
  std::vector<NetDeviceContainer> lanRouterPorts (nBackbone);
  for (uint32_t i = 0; i < nBackbone; ++i)
    {
      NodeContainer nodesInLan;
      nodesInLan.Add (backboneRouters.Get(i)); // The backbone router
      nodesInLan.Add (lanNodes[i]);

      lanDevices[i] = csma.Install (nodesInLan);
      // Extract just the router port (first device)
      NetDeviceContainer routerPort;
      routerPort.Add (lanDevices[i].Get(0));
      lanRouterPorts[i] = routerPort;
    }

  // 4. Attach a WiFi infrastructure to each BBR
  YansWifiPhyHelper infraPhy = YansWifiPhyHelper::Default ();
  YansWifiChannelHelper infraChannel = YansWifiChannelHelper::Default ();
  infraPhy.SetChannel (infraChannel.Create ());

  WifiHelper infraWifi;
  infraWifi.SetStandard (WIFI_STANDARD_80211b);
  WifiMacHelper infraMac;

  std::vector<NetDeviceContainer> infraApDevices (nBackbone);
  std::vector<NetDeviceContainer> infraStaDevices (nBackbone);

  for (uint32_t i = 0; i < nBackbone; ++i)
    {
      std::ostringstream ss;
      ss << "infra-ssid-" << i;
      Ssid infraSsid = Ssid (ss.str ());

      // AP: the backbone router node
      infraMac.SetType ("ns3::ApWifiMac",
                        "Ssid", SsidValue (infraSsid));
      NetDeviceContainer apDev = infraWifi.Install (infraPhy, infraMac, apNodes[i]);

      // STAs
      infraMac.SetType ("ns3::StaWifiMac",
                        "Ssid", SsidValue (infraSsid),
                        "ActiveProbing", BooleanValue (false));
      NetDeviceContainer staDevs = infraWifi.Install (infraPhy, infraMac, wifiStaNodes[i]);
      infraApDevices[i] = apDev;
      infraStaDevices[i] = staDevs;
    }

  // 5. Assign IP addresses
  Ipv4AddressHelper address;

  // Backbone network (ad hoc WiFi)
  address.SetBase ("10.0.0.0", "255.255.255.0");
  Ipv4InterfaceContainer backboneIfaces = address.Assign (backboneWifiDevices);

  // Each LAN: 10.<i+1>.0.0/24
  for (uint32_t i = 0; i < nBackbone; ++i)
    {
      std::ostringstream subnet;
      subnet << "10." << (i+1) << ".0.0";
      address.SetBase (subnet.str ().c_str (), "255.255.255.0");
      lanIfaces[i] = address.Assign (lanDevices[i]);
    }

  // Each infra WiFi: 10.(i+1).1.0/24
  for (uint32_t i = 0; i < nBackbone; ++i)
    {
      std::ostringstream subnet;
      subnet << "10." << (i+1) << ".1.0";
      address.SetBase (subnet.str ().c_str (), "255.255.255.0");
      wifiStaIfaces[i] = address.Assign (infraStaDevices[i]);
      wifiApIfaces[i] = address.Assign (infraApDevices[i]);
    }

  // 6. Mobility
  // BBRs - fixed row
  Ptr<ListPositionAllocator> bbrPos = CreateObject<ListPositionAllocator> ();
  for (uint32_t i = 0; i < nBackbone; ++i)
    {
      bbrPos->Add (Vector (50.0 + i * 100.0, 100.0, 0.0)); // spaced 100m apart
    }
  MobilityHelper bbrMobility;
  bbrMobility.SetPositionAllocator (bbrPos);
  bbrMobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  bbrMobility.Install (backboneRouters);

  // LAN nodes - fixed, arrayed behind the BBRs
  for (uint32_t i = 0; i < nBackbone; ++i)
    {
      MobilityHelper lanMob;
      Ptr<ListPositionAllocator> lanPos = CreateObject<ListPositionAllocator> ();
      for (uint32_t j = 0; j < nLan; ++j)
        {
          lanPos->Add (Vector (50.0 + i*100.0 + 10.0*(j+1), 80.0, 0.0)); // 10m steps behind each BBR
        }
      lanMob.SetPositionAllocator (lanPos);
      lanMob.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
      lanMob.Install (lanNodes[i]);
    }

  // WiFi STAs - random walk near each BBR AP
  for (uint32_t i = 0; i < nBackbone; ++i)
    {
      MobilityHelper wifiStaMob;
      wifiStaMob.SetPositionAllocator ("ns3::RandomRectanglePositionAllocator",
                                       "X", StringValue ("ns3::UniformRandomVariable[Min=30.0|Max=70.0]"),
                                       "Y", StringValue ("ns3::UniformRandomVariable[Min=110.0|Max=150.0]"));
      wifiStaMob.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                                   "Bounds", RectangleValue (Rectangle (0, 300, 0, 300)),
                                   "Speed", StringValue ("ns3::ConstantRandomVariable[Constant=1.0]"));
      wifiStaMob.Install (wifiStaNodes[i]);
    }

  // APs: fixed (the BBR nodes)
  // Already handled by bbrMobility

  // Course Change Callbacks for WiFi STAs
  if (enableCourseChange)
    {
      for (uint32_t i = 0; i < nBackbone; ++i)
        {
          for (uint32_t j = 0; j < nWifi; ++j)
            {
              Ptr<Node> node = wifiStaNodes[i].Get(j);
              Ptr<MobilityModel> mob = node->GetObject<MobilityModel> ();
              std::ostringstream oss;
              oss << "/NodeList/" << node->GetId () << "/$ns3::MobilityModel/CourseChange";
              Config::Connect (oss.str (), MakeBoundCallback (&CourseChange, oss.str ()));
            }
        }
    }

  // 7. Application: UDP flow from wired LAN node (first BBR, first LAN node) to WiFi STA (last BBR, last STA)
  uint16_t port = 9;
  Ptr<Node> srcNode = lanNodes[0].Get (0); // first LAN node on BBR 0
  Ptr<Node> dstNode = wifiStaNodes[nBackbone - 1].Get (nWifi - 1); // last STA on the last BBR

  // Install UDP server on destination
  UdpServerHelper udpServer (port);
  ApplicationContainer serverApp = udpServer.Install (dstNode);
  serverApp.Start (Seconds (1.0));
  serverApp.Stop (Seconds (simTime-0.5));

  // UDP client on source
  Ipv4Address dstAddr = wifiStaIfaces[nBackbone - 1].GetAddress (nWifi - 1);
  UdpClientHelper udpClient (dstAddr, port);
  udpClient.SetAttribute ("MaxPackets", UintegerValue (10000));
  udpClient.SetAttribute ("Interval", TimeValue (MilliSeconds (50)));
  udpClient.SetAttribute ("PacketSize", UintegerValue (512));

  ApplicationContainer clientApp = udpClient.Install (srcNode);
  clientApp.Start (Seconds (2.0));
  clientApp.Stop (Seconds (simTime-0.5));

  // 8. Enable tracing
  if (tracing)
    {
      phy.EnablePcapAll ("backbone-adhoc", false);
      csma.EnablePcapAll ("lan-wired", false);
      infraPhy.EnablePcapAll ("infra-wifi", false);
    }

  // 9. Routing: populate
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // 10. Flow Monitor
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  Simulator::Stop (Seconds (simTime));
  Simulator::Run ();

  // Optionally, dump flow monitor data
  monitor->SerializeToXmlFile ("mixed-wired-wireless-flowmon.xml", true, true);

  Simulator::Destroy ();
  return 0;
}