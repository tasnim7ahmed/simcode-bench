#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/spectrum-wifi-helper.h"
#include "ns3/wifi-mac-helper.h"
#include "ns3/ssid.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/traffic-control-module.h"
#include <vector>
#include <sstream>
#include <iomanip>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("Wifi80211acParamSweep");

struct Params
{
  std::string transport;   // "UDP" or "TCP"
  bool rtsCts;
  uint16_t channelWidth;   // 20, 40, 80, 160
  uint8_t mcs;             // 0-9
  bool shortGuard;         // short or long guard interval
  double distance;         // meters
};

// Generate all parameter combinations
std::vector<Params> GenerateParamCombinations ()
{
  std::vector<Params> combos;
  std::vector<std::string> transports = {"UDP", "TCP"};
  std::vector<bool> rtscts_vec = {false, true};
  std::vector<uint16_t> channelWidths = {20, 40, 80, 160};
  std::vector<uint8_t> mcsVec = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
  std::vector<bool> giVec = {false, true}; // false=long, true=short
  std::vector<double> distances = {2.0}; // you can add more distances if desired

  for (const auto& t : transports)
    for (bool rts : rtscts_vec)
      for (auto w : channelWidths)
        for (auto m : mcsVec)
          for (bool gi : giVec)
            for (auto d : distances)
              {
                Params p;
                p.transport = t;
                p.rtsCts = rts;
                p.channelWidth = w;
                p.mcs = m;
                p.shortGuard = gi;
                p.distance = d;
                combos.push_back (p);
              }
  return combos;
}

// Goodput calculation app: for UDP, measure in server; for TCP, in sink
struct GoodputResult
{
  Params params;
  double rxBytes;
  double duration;
};

GoodputResult
RunSimulation (const Params& par)
{
  uint32_t payloadSize = 1472;
  double simulationTime = 10.0; // seconds
  uint16_t port = 5009;

  NodeContainer wifiStaNode, wifiApNode;
  wifiStaNode.Create (1);
  wifiApNode.Create (1);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  SpectrumWifiPhyHelper phy = SpectrumWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  phy.Set ("ChannelWidth", UintegerValue (par.channelWidth));
  if (par.channelWidth == 20)
    phy.Set ("Frequency", UintegerValue (5180));
  else if (par.channelWidth == 40)
    phy.Set ("Frequency", UintegerValue (5200));
  else if (par.channelWidth == 80)
    phy.Set ("Frequency", UintegerValue (5210));
  else
    phy.Set ("Frequency", UintegerValue (5210));

  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211ac);

  WifiMacHelper mac;
  Ssid ssid = Ssid ("ns3-80211ac");

  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
      "DataMode", StringValue ("VhtMcs" + std::to_string (par.mcs)),
      "ControlMode", StringValue ("VhtMcs0"),
      "RtsCtsThreshold", UintegerValue (par.rtsCts ? 0 : 9999));

  phy.Set ("ShortGuardEnabled", BooleanValue (par.shortGuard));
  phy.Set ("GuardInterval", TimeValue (NanoSeconds (par.shortGuard ? 400 : 800)));

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
  positionAlloc->Add (Vector (par.distance, 0.0, 0.0));
  mobility.SetPositionAllocator (positionAlloc);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (wifiStaNode);
  mobility.Install (wifiApNode);

  InternetStackHelper stack;
  stack.Install (wifiStaNode);
  stack.Install (wifiApNode);

  Ipv4AddressHelper address;
  address.SetBase ("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer staIf = address.Assign (staDevice);
  Ipv4InterfaceContainer apIf = address.Assign (apDevice);

  ApplicationContainer serverApp, clientApp;
  Ptr<Application> sinkApp;
  double stopTime = simulationTime + 1.0;

  if (par.transport == "UDP")
    {
      UdpServerHelper server (port);
      serverApp = server.Install (wifiApNode.Get (0));
      serverApp.Start (Seconds (0.0));
      serverApp.Stop (Seconds (stopTime));

      UdpClientHelper client (apIf.GetAddress (0), port);
      client.SetAttribute ("MaxPackets", UintegerValue ((uint32_t)-1));
      client.SetAttribute ("Interval", TimeValue (Seconds (0.00001)));
      client.SetAttribute ("PacketSize", UintegerValue (payloadSize));
      clientApp = client.Install (wifiStaNode.Get (0));
      clientApp.Start (Seconds (1.0));
      clientApp.Stop (Seconds (stopTime));
    }
  else // TCP
    {
      uint16_t tcpPort = port;
      Address sinkAddress (InetSocketAddress (apIf.GetAddress (0), port));
      PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), tcpPort));
      serverApp = sinkHelper.Install (wifiApNode.Get (0));
      sinkApp = serverApp.Get (0);
      serverApp.Start (Seconds (0.0));
      serverApp.Stop (Seconds (stopTime));

      OnOffHelper client ("ns3::TcpSocketFactory", sinkAddress);
      client.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
      client.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
      client.SetAttribute ("DataRate", DataRateValue (DataRate ("1Gbps")));
      client.SetAttribute ("PacketSize", UintegerValue (payloadSize));
      clientApp = client.Install (wifiStaNode.Get (0));
      clientApp.Start (Seconds (1.0));
      clientApp.Stop (Seconds (stopTime));
    }

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  Simulator::Stop (Seconds (stopTime));
  Simulator::Run ();

  double rxBytes = 0;
  if (par.transport == "UDP")
    {
      Ptr<UdpServer> udpServer = DynamicCast<UdpServer> (serverApp.Get (0));
      rxBytes = udpServer->GetReceived () * payloadSize;
    }
  else
    {
      Ptr<PacketSink> tcpSink = DynamicCast<PacketSink> (serverApp.Get (0));
      rxBytes = tcpSink->GetTotalRx ();
    }
  Simulator::Destroy ();

  GoodputResult result;
  result.params = par;
  result.rxBytes = rxBytes;
  result.duration = simulationTime;
  return result;
}

std::string
BoolToStr (bool val)
{
  return val ? "YES" : "NO";
}

int
main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  std::vector<Params> combos = GenerateParamCombinations ();

  std::cout << "transport\trtscts\tchannel_width\tmcs\tshort_guard\tdistance(goodput_mbps)\n";

  for (const auto& p : combos)
    {
      GoodputResult res = RunSimulation (p);
      double goodput = (res.rxBytes * 8.0) / (1e6 * res.duration);
      std::cout << p.transport << "\t"
                << BoolToStr (p.rtsCts) << "\t"
                << p.channelWidth << "\t"
                << unsigned (p.mcs) << "\t"
                << BoolToStr (p.shortGuard) << "\t"
                << std::fixed << std::setprecision(2) << p.distance << "(" << goodput << ")\n";
      std::cout.flush ();
    }

  return 0;
}