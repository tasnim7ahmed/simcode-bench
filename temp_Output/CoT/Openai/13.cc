#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/olsr-helper.h"
#include "ns3/bridge-helper.h"
#include "ns3/csma-helper.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/ipv4-address-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("MixedWiredWirelessSimulation");

void CourseChangeCallback (std::string context, Ptr<const MobilityModel> mobility)
{
  Vector pos = mobility->GetPosition ();
  NS_LOG_UNCOND (Simulator::Now ().GetSeconds () << "s " << context <<
                 " mobility: x=" << pos.x << ", y=" << pos.y << ", z=" << pos.z);
}

int main (int argc, char *argv[])
{
  uint32_t nBackboneRouters = 3;
  uint32_t nLanNodesPerRouter = 2;
  uint32_t nInfraStasPerRouter = 2;
  double simTime = 20.0;
  std::string phyMode = "HtMcs7";
  bool enableTracing = true;
  bool enableMobilityTracing = true;

  CommandLine cmd;
  cmd.AddValue ("nBackboneRouters", "Number of backbone routers", nBackboneRouters);
  cmd.AddValue ("nLanNodesPerRouter", "Number of LAN nodes per backbone router", nLanNodesPerRouter);
  cmd.AddValue ("nInfraStasPerRouter", "Number of wifi sta nodes per backbone router", nInfraStasPerRouter);
  cmd.AddValue ("simTime", "Simulation time (s)", simTime);
  cmd.AddValue ("enableTracing", "Enable pcap/Ascii tracing", enableTracing);
  cmd.AddValue ("enableMobilityTracing", "Enable mobility course change callbacks", enableMobilityTracing);
  cmd.Parse (argc, argv);

  // Backbone routers
  NodeContainer backboneRouters;
  backboneRouters.Create (nBackboneRouters);

  // For each backbone router: local LAN nodes and local WiFi infra nodes
  std::vector<NodeContainer> lanNodes (nBackboneRouters);
  std::vector<NodeContainer> wifiApNodes (nBackboneRouters);
  std::vector<NodeContainer> wifiStaNodes (nBackboneRouters);

  for (uint32_t i = 0; i < nBackboneRouters; ++i)
    {
      // Each LAN node set includes only the LAN nodes (not the router itself)
      lanNodes[i].Create (nLanNodesPerRouter);
      // Each WiFi AP is a separate node for bridging convenience
      wifiApNodes[i].Create (1);
      wifiStaNodes[i].Create (nInfraStasPerRouter);
    }

  // Ad hoc wifi network among backbone routers
  YansWifiChannelHelper adhocChannel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper adhocPhy = YansWifiPhyHelper::Default ();
  adhocPhy.SetChannel (adhocChannel.Create ());

  WifiHelper adhocWifi;
  adhocWifi.SetStandard (WIFI_STANDARD_80211n);
  WifiMacHelper adhocMac;
  adhocWifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager", "DataMode", StringValue (phyMode), "ControlMode", StringValue ("HtMcs0"));
  adhocMac.SetType ("ns3::AdhocWifiMac");

  NetDeviceContainer adhocDevices = adhocWifi.Install (adhocPhy, adhocMac, backboneRouters);

  // For each router, set up 802.11 infra
  std::vector<NetDeviceContainer> infraApDevices (nBackboneRouters);
  std::vector<NetDeviceContainer> infraStaDevices (nBackboneRouters);

  for (uint32_t i = 0; i < nBackboneRouters; ++i)
    {
      YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
      YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
      phy.SetChannel (channel.Create ());
      WifiHelper wifi;
      wifi.SetStandard (WIFI_STANDARD_80211n);
      wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager","DataMode", StringValue (phyMode), "ControlMode", StringValue ("HtMcs0"));

      Ssid ssid = Ssid ("infra-ssid-" + std::to_string (i));
      WifiMacHelper mac;

      // AP
      mac.SetType ("ns3::ApWifiMac", "Ssid", SsidValue (ssid));
      infraApDevices[i] = wifi.Install (phy, mac, wifiApNodes[i]);

      // STAs
      mac.SetType ("ns3::StaWifiMac", "Ssid", SsidValue (ssid));
      infraStaDevices[i] = wifi.Install (phy, mac, wifiStaNodes[i]);
    }

  // Each backbone router gets a CSMA LAN including itself
  CsmaHelper csmaLan;
  csmaLan.SetChannelAttribute ("DataRate", StringValue ("100Mbps"));
  csmaLan.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (6560)));

  std::vector<NodeContainer> csmaLanNodes (nBackboneRouters);
  std::vector<NetDeviceContainer> csmaLanDevices (nBackboneRouters);

  for (uint32_t i = 0; i < nBackboneRouters; ++i)
    {
      // The LAN is: backbone router + local LAN nodes
      csmaLanNodes[i].Add (backboneRouters.Get (i));
      csmaLanNodes[i].Add (lanNodes[i]);
      csmaLanDevices[i] = csmaLan.Install (csmaLanNodes[i]);
    }

  // Use Bridge to connect AP node and backbone router (for infra network)
  BridgeHelper bridge;
  std::vector<NetDeviceContainer> bridgeDevices (nBackboneRouters);

  for (uint32_t i = 0; i < nBackboneRouters; ++i)
    {
      // Create a NetDevice: AP + backbone router
      NodeContainer bridgeNodes;
      bridgeNodes.Add (backboneRouters.Get (i));
      bridgeNodes.Add (wifiApNodes[i].Get (0));
      NetDeviceContainer bridgePorts;
      // backbone router gets wifi device to connect to AP
      YansWifiChannelHelper phychan = YansWifiChannelHelper::Default ();
      YansWifiPhyHelper brPhy = YansWifiPhyHelper::Default ();
      brPhy.SetChannel (phychan.Create ());
      WifiHelper brWifi;
      brWifi.SetStandard (WIFI_STANDARD_80211n);
      brWifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager", "DataMode", StringValue (phyMode), "ControlMode", StringValue ("HtMcs0"));
      Ssid ssid = Ssid ("infra-ssid-" + std::to_string (i));
      WifiMacHelper brmac;
      brmac.SetType ("ns3::StaWifiMac", "Ssid", SsidValue (ssid));
      NetDeviceContainer bridgeStaDevice = brWifi.Install (brPhy, brmac, backboneRouters.Get (i));
      // Bridge together AP's wifi device and csma device on backbone router
      NetDeviceContainer devicesToBridge;
      devicesToBridge.Add (infraApDevices[i].Get (0)); // AP's wifi device
      devicesToBridge.Add (csmaLanDevices[i].Get (0)); // backbone router's CSMA device
      devicesToBridge.Add (bridgeStaDevice.Get (0));    // backbone router's wifi infra device
      bridgeDevices[i] = bridge.Install (backboneRouters.Get (i), devicesToBridge);
    }

  // Install stacks and OLSR on backbone routers; all others get normal stack
  InternetStackHelper stack, olsrStack;
  OlsrHelper olsr;
  Ipv4StaticRoutingHelper staticRouting;
  stack.SetRoutingHelper (staticRouting);
  olsrStack.SetRoutingHelper (olsr);

  // Backbone routers: OLSR, everyone else: plain
  olsrStack.Install (backboneRouters);
  for (uint32_t i = 0; i < nBackboneRouters; ++i)
    {
      stack.Install (lanNodes[i]);
      stack.Install (wifiApNodes[i]);
      stack.Install (wifiStaNodes[i]);
    }

  // Assign IP addresses
  Ipv4AddressHelper adhocIp;
  adhocIp.SetBase ("10.0.0.0", "255.255.255.0");
  Ipv4InterfaceContainer adhocIfs = adhocIp.Assign (adhocDevices);

  std::vector<Ipv4InterfaceContainer> lanInterfaces (nBackboneRouters);
  std::vector<Ipv4InterfaceContainer> wifiInfraStaInterfaces (nBackboneRouters);
  std::vector<Ipv4InterfaceContainer> wifiInfraApInterfaces (nBackboneRouters);

  for (uint32_t i = 0; i < nBackboneRouters; ++i)
    {
      Ipv4AddressHelper lanAddr;
      lanAddr.SetBase ("10.1." + std::to_string(i) + ".0", "255.255.255.0");
      lanInterfaces[i] = lanAddr.Assign (csmaLanDevices[i]);
      // Not all APs need IP, but for management we assign one
      Ipv4AddressHelper wifiInfraAddr;
      wifiInfraAddr.SetBase ("10.2." + std::to_string(i) + ".0", "255.255.255.0");
      wifiInfraApInterfaces[i] = wifiInfraAddr.Assign (infraApDevices[i]);
      wifiInfraStaInterfaces[i] = wifiInfraAddr.Assign (infraStaDevices[i]);
    }

  // Mobility — backbone routers fixed in grid, LAN/wifi STA random walk
  MobilityHelper mobility;

  // Backbone routers spaced along a line
  Ptr<ListPositionAllocator> pos = CreateObject<ListPositionAllocator> ();
  for (uint32_t i = 0; i < nBackboneRouters; ++i)
    pos->Add (Vector (50*i, 100, 0));
  mobility.SetPositionAllocator (pos);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (backboneRouters);

  // LAN nodes in clusters next to routers
  for (uint32_t i = 0; i < nBackboneRouters; ++i)
    {
      MobilityHelper lanMob;
      Ptr<ListPositionAllocator> p = CreateObject<ListPositionAllocator> ();
      for (uint32_t j = 0; j < nLanNodesPerRouter; ++j)
        p->Add (Vector (50*i + 10 + j*3, 95, 0));
      lanMob.SetPositionAllocator (p);
      lanMob.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
      lanMob.Install (lanNodes[i]);
    }

  // WiFi infra APs positioned close to backbone routers
  for (uint32_t i = 0; i < nBackboneRouters; ++i)
    {
      MobilityHelper apMob;
      Ptr<ListPositionAllocator> p = CreateObject<ListPositionAllocator> ();
      p->Add (Vector (50*i, 110, 0));
      apMob.SetPositionAllocator (p);
      apMob.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
      apMob.Install (wifiApNodes[i]);
    }

  // WiFi infra STAs: random walk within a rectangle near AP
  for (uint32_t i = 0; i < nBackboneRouters; ++i)
    {
      MobilityHelper staMob;
      staMob.SetPositionAllocator ("ns3::RandomRectanglePositionAllocator",
                                   "X", StringValue ("ns3::UniformRandomVariable[Min=" + std::to_string(50*i-15) + "|Max=" + std::to_string(50*i+15) + "]"),
                                   "Y", StringValue ("ns3::UniformRandomVariable[Min=90|Max=130]"));
      staMob.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                               "Bounds", RectangleValue (Rectangle (50*i-15, 50*i+15, 90, 130)));
      staMob.Install (wifiStaNodes[i]);
    }

  // Mobility tracing
  if (enableMobilityTracing)
    {
      for (NodeContainer *c : {&backboneRouters})
        for (uint32_t i=0; i<c->GetN(); ++i)
          {
            Ptr<Node> n = c->Get(i);
            Ptr<MobilityModel> mob = n->GetObject<MobilityModel>();
            if (mob)
              {
                std::ostringstream context;
                context << "/NodeList/" << n->GetId() << "/$ns3::MobilityModel/CourseChange";
                Config::ConnectWithoutContext (context.str(), MakeBoundCallback (&CourseChangeCallback));
              }
          }
      for (uint32_t i = 0; i < nBackboneRouters; ++i)
        {
          for (NodeContainer *c : {&lanNodes[i], &wifiStaNodes[i]})
            {
              for (uint32_t j=0; j<c->GetN(); ++j)
                {
                  Ptr<Node> n = c->Get(j);
                  Ptr<MobilityModel> mob = n->GetObject<MobilityModel>();
                  if (mob)
                    {
                      std::ostringstream context;
                      context << "/NodeList/" << n->GetId() << "/$ns3::MobilityModel/CourseChange";
                      Config::ConnectWithoutContext (context.str(), MakeBoundCallback (&CourseChangeCallback));
                    }
                }
            }
          for (uint32_t j = 0; j < wifiApNodes[i].GetN (); ++j)
            {
              Ptr<Node> n = wifiApNodes[i].Get (j);
              Ptr<MobilityModel> mob = n->GetObject<MobilityModel> ();
              if (mob)
                {
                  std::ostringstream context;
                  context << "/NodeList/" << n->GetId()
                          << "/$ns3::MobilityModel/CourseChange";
                  Config::ConnectWithoutContext (context.str (), MakeBoundCallback (&CourseChangeCallback));
                }
            }
        }
    }

  // Tracing/pkt capture
  if (enableTracing)
    {
      csmaLan.EnablePcapAll ("mixed-wired-wireless-lan");
      for (uint32_t i = 0; i < nBackboneRouters; ++i)
        {
          std::ostringstream oss;
          oss << "mixed-wired-wireless-wifi-infra" << i;
          YansWifiPhyHelper::Default ().EnablePcap (oss.str ().c_str (), infraApDevices[i].Get (0), true);
        }
    }

  // UDP app: one LAN node in first router to one last AP's wifi STA
  uint16_t udpPort = 9;

  ApplicationContainer serverApps, clientApps;
  // Pick last infra STA as destination
  Ptr<Node> lastSta = wifiStaNodes[nBackboneRouters-1].Get (nInfraStasPerRouter-1);
  Ipv4Address destAddr = wifiInfraStaInterfaces[nBackboneRouters-1].GetAddress (nInfraStasPerRouter-1);

  UdpServerHelper udpServer (udpPort);
  serverApps.Add (udpServer.Install (lastSta));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (simTime-1));

  // Pick first LAN node as source
  Ptr<Node> srcNode = lanNodes[0].Get (0);
  UdpClientHelper udpClient (destAddr, udpPort);
  udpClient.SetAttribute ("MaxPackets", UintegerValue (2000));
  udpClient.SetAttribute ("Interval", TimeValue (MilliSeconds (10)));
  udpClient.SetAttribute ("PacketSize", UintegerValue (512));
  clientApps.Add (udpClient.Install (srcNode));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (simTime-1));

  Simulator::Stop (Seconds (simTime));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}