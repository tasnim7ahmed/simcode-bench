#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/ssid.h"
#include "ns3/wifi-phy.h"
#include "ns3/wifi-standards.h"
#include "ns3/wifi-module.h"
#include "ns3/config-store-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("Wifi80211nGoodputSim");

struct SimulationParams
{
  uint16_t mcs = 0;
  uint32_t channelWidth = 20;
  bool shortGuardInterval = false;
  double simulationTime = 10.0;
  std::string transport = "UDP";
  bool rtsCts = false;
  double distance = 5.0;
  double offeredThroughput = 50.0;
};

static void
CalculateGoodput (Ptr<PacketSink> sink, Time startTime, std::string transport)
{
  static uint64_t lastTotalRx = 0;
  uint64_t totalRx = sink->GetTotalRx ();
  double goodput = (totalRx - lastTotalRx) * 8.0 / 1e6; // Mbps per interval
  lastTotalRx = totalRx;
  Time now = Simulator::Now ();
  NS_LOG_UNCOND ("[" << now.GetSeconds ()
                     << "s] " << transport    << " Goodput: "
                     << goodput << " Mbps");
}

int
main (int argc, char *argv[])
{
  SimulationParams params;

  CommandLine cmd;
  cmd.AddValue ("mcs", "MCS index (0-7)", params.mcs);
  cmd.AddValue ("channelWidth", "Channel width in MHz (20 or 40)", params.channelWidth);
  cmd.AddValue ("shortGuardInterval", "Enable Short Guard Interval", params.shortGuardInterval);
  cmd.AddValue ("simulationTime", "Simulation time in seconds", params.simulationTime);
  cmd.AddValue ("transport", "Transport protocol (UDP or TCP)", params.transport);
  cmd.AddValue ("rtsCts", "Enable RTS/CTS", params.rtsCts);
  cmd.AddValue ("distance", "Distance between AP and STA in meters", params.distance);
  cmd.AddValue ("offeredThroughput", "Offered throughput in Mbps", params.offeredThroughput);
  cmd.Parse (argc, argv);

  if (params.mcs > 7)
    {
      NS_ABORT_MSG ("MCS index must be between 0 and 7 for 802.11n single spatial stream.");
    }
  if ((params.channelWidth != 20) && (params.channelWidth != 40))
    {
      NS_ABORT_MSG ("Channel width must be 20 or 40 MHz.");
    }

  NodeContainer wifiStaNode, wifiApNode;
  wifiStaNode.Create (1);
  wifiApNode.Create (1);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());
  phy.Set ("ShortGuardEnabled", BooleanValue (params.shortGuardInterval));
  phy.Set ("ChannelWidth", UintegerValue (params.channelWidth));

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211n_5GHZ);
  std::ostringstream oss;
  oss << "HtMcs" << params.mcs;
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                               "DataMode", StringValue (oss.str ()),
                               "ControlMode", StringValue ("HtMcs0"),
                               "RtsCtsThreshold", UintegerValue (params.rtsCts ? 0 : 2347));

  WifiMacHelper mac;
  Ssid ssid = Ssid ("wifi-80211n");

  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));
  NetDeviceContainer staDevice = wifi.Install (phy, mac, wifiStaNode);

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid));
  NetDeviceContainer apDevice = wifi.Install (phy, mac, wifiApNode);

  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  positionAlloc->Add (Vector (0.0, 0.0, 0.0));
  positionAlloc->Add (Vector (params.distance, 0.0, 0.0));
  mobility.SetPositionAllocator (positionAlloc);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (wifiApNode);
  mobility.Install (wifiStaNode);

  InternetStackHelper stack;
  stack.Install (wifiApNode);
  stack.Install (wifiStaNode);

  Ipv4AddressHelper address;
  address.SetBase ("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterface = address.Assign (apDevice);
  Ipv4InterfaceContainer staInterface = address.Assign (staDevice);

  ApplicationContainer serverApp, clientApp;
  Ptr<PacketSink> sink;

  uint16_t port = 5000;
  double offeredLoad = params.offeredThroughput * 1000000.0/8.0; // bytes/s

  if (params.transport == "UDP")
    {
      UdpServerHelper server (port);
      serverApp = server.Install (wifiStaNode.Get (0));
      serverApp.Start (Seconds (0.0));
      serverApp.Stop (Seconds (params.simulationTime + 1.0));

      UdpClientHelper client (staInterface.GetAddress (0), port);
      client.SetAttribute ("MaxPackets", UintegerValue (0));
      client.SetAttribute ("Interval", TimeValue (Seconds (0.0))); // unlimited
      uint32_t pktSize = 1472;
      double pps = offeredLoad / pktSize;
      client.SetAttribute ("PacketSize", UintegerValue (pktSize));
      client.SetAttribute ("Interval", TimeValue (Seconds (1.0/pps)));
      clientApp = client.Install (wifiApNode.Get (0));
      clientApp.Start (Seconds (1.0));
      clientApp.Stop (Seconds (params.simulationTime + 1.0));

      sink = DynamicCast<PacketSink> (serverApp.Get (0));
    }
  else if (params.transport == "TCP")
    {
      uint32_t pktSize = 1448;
      PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory",
                                   Address (InetSocketAddress (staInterface.GetAddress (0), port)));
      serverApp = sinkHelper.Install (wifiStaNode.Get (0));
      serverApp.Start (Seconds (0.0));
      serverApp.Stop (Seconds (params.simulationTime + 1.0));

      OnOffHelper onoff ("ns3::TcpSocketFactory",
                         Address (InetSocketAddress (staInterface.GetAddress (0), port)));
      onoff.SetAttribute ("DataRate", DataRateValue (DataRate ((uint64_t) (params.offeredThroughput * 1000000))));
      onoff.SetAttribute ("PacketSize", UintegerValue (pktSize));
      onoff.SetAttribute ("StartTime", TimeValue (Seconds (1.0)));
      onoff.SetAttribute ("StopTime", TimeValue (Seconds (params.simulationTime + 1.0)));
      clientApp = onoff.Install (wifiApNode.Get (0));
      sink = DynamicCast<PacketSink> (serverApp.Get (0));
    }
  else
    {
      NS_ABORT_MSG ("Invalid transport type: " << params.transport << " (choose UDP or TCP)");
    }

  Simulator::Schedule (Seconds (1.0),
      &CalculateGoodput, sink, Seconds (1.0), params.transport);
  for (double t = 2.0; t <= params.simulationTime; t += 1.0)
    {
      Simulator::Schedule (Seconds (t),
          &CalculateGoodput, sink, Seconds (t), params.transport);
    }

  Simulator::Stop (Seconds (params.simulationTime + 1.5));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}