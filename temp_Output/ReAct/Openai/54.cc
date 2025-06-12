#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("Wifi80211acGoodputExperiment");

std::string GeneratePhyMode(uint8_t mcs, bool shortGi)
{
  std::ostringstream oss;
  oss << "VhtMcs" << unsigned(mcs);
  if (shortGi)
    oss << "ShortGi";
  return oss.str();
}

int main (int argc, char *argv[])
{
  // Parameters (can be changed as needed)
  double distance = 5.0;
  double simulationTime = 10.0; // seconds
  std::string transport = "UDP"; // "UDP" or "TCP"
  uint16_t port = 9;

  // 802.11ac configs
  std::vector<uint32_t> channelWidths = {20, 40, 80, 160, 160}; // add 80+80 as the second 160 (we check below)
  std::vector<std::string> channelWidthNames = {"20MHz", "40MHz", "80MHz", "160MHz", "80+80MHz"};
  bool channelWidthIs8080[5] = {false, false, false, false, true};

  std::vector<uint8_t> mcsList = {0,1,2,3,4,5,6,7,8,9};
  std::vector<bool> shortGis = {false, true};
  std::vector<bool> rtsCtsList = {false,true};

  CommandLine cmd;
  cmd.AddValue ("distance", "Distance between AP and STA (meters)", distance);
  cmd.AddValue ("transport", "UDP or TCP", transport);
  cmd.AddValue ("simulationTime", "Simulation time (seconds)", simulationTime);
  cmd.Parse (argc,argv);

  std::cout << "channelWidth,gi,rtscts,mcs,goodput_Mbps\n";

  for (uint32_t cwi = 0; cwi < channelWidths.size(); ++cwi)
  {
    uint32_t cw = channelWidths[cwi];
    for (bool shortGi : shortGis)
    {
      for (bool rtsCts : rtsCtsList)
      {
        for (uint8_t mcs : mcsList)
        {
          // Clean up between runs
          NodeContainer wifiStaNode, wifiApNode;
          wifiStaNode.Create (1);
          wifiApNode.Create (1);

          YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
          SpectrumWifiPhyHelper phy = SpectrumWifiPhyHelper::Default ();
          phy.SetChannel (channel.Create ());

          WifiHelper wifi;
          wifi.SetStandard (WIFI_STANDARD_80211ac);

          WifiMacHelper mac;

          // MCS, GI, and channel width settings
          std::string phyMode = GeneratePhyMode(mcs, shortGi);

          VhtWifiMacHelper vhtMac = VhtWifiMacHelper::Default ();
          VhtWifiPhyHelper vhtPhy = VhtWifiPhyHelper::Default ();
          vhtPhy.SetChannel (channel.Create ());

          vhtPhy.Set ("ChannelWidth", UintegerValue (cw));
          if (channelWidthIs8080[cwi])
          {
            vhtPhy.Set ("ChannelWidth", UintegerValue (80)); // 80+80 means 80 MHz non-contiguous, simulate as 160 MHz
            vhtPhy.Set ("ChannelNumber", UintegerValue (42));
            vhtPhy.Set ("Secondary80Channel", UintegerValue (155));
          }
          vhtPhy.Set ("ShortGuardEnabled", BooleanValue (shortGi));
          vhtPhy.Set ("GuardInterval", TimeValue (NanoSeconds (shortGi ? 400 : 800)));

          // Set PHY attributes for both AP and STA
          vhtPhy.Set ("TxPowerStart", DoubleValue (23.0));
          vhtPhy.Set ("TxPowerEnd", DoubleValue (23.0));

          HtWifiMacHelper htMac;
          wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                        "DataMode", StringValue (phyMode),
                                        "ControlMode", StringValue (phyMode),
                                        "RtsCtsThreshold", UintegerValue (rtsCts ? 0 : 99999));

          Ssid ssid = Ssid ("ns3-80211ac");

          // STA
          mac.SetType ("ns3::StaWifiMac",
                       "Ssid", SsidValue (ssid),
                       "ActiveProbing", BooleanValue (false));
          NetDeviceContainer staDevice = wifi.Install (phy, mac, wifiStaNode);

          // AP
          mac.SetType ("ns3::ApWifiMac",
                       "Ssid", SsidValue (ssid));
          NetDeviceContainer apDevice = wifi.Install (phy, mac, wifiApNode);

          // Mobility
          MobilityHelper mobility;
          Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
          positionAlloc->Add (Vector (0.0, 0.0, 0.0)); // AP
          positionAlloc->Add (Vector (distance, 0.0, 0.0)); // STA
          mobility.SetPositionAllocator (positionAlloc);
          mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
          mobility.Install (wifiApNode);
          mobility.Install (wifiStaNode);

          // Internet stack
          InternetStackHelper stack;
          stack.Install (wifiApNode);
          stack.Install (wifiStaNode);

          Ipv4AddressHelper address;
          address.SetBase ("10.1.1.0", "255.255.255.0");
          Ipv4InterfaceContainer apInterface = address.Assign (apDevice);
          Ipv4InterfaceContainer staInterface = address.Assign (staDevice);

          // Connect applications
          ApplicationContainer serverApp, clientApp;
          uint32_t payloadSize = 1472;
          double startTime = 1.0;
          double stopTime = simulationTime;

          if (transport == "UDP")
          {
            UdpServerHelper server (port);
            serverApp = server.Install (wifiApNode.Get (0));
            serverApp.Start (Seconds (startTime));
            serverApp.Stop (Seconds (stopTime));

            UdpClientHelper client (apInterface.GetAddress (0), port);
            client.SetAttribute ("MaxPackets", UintegerValue (4294967295u));
            client.SetAttribute ("Interval", TimeValue (Time ("0.00001"))); // as fast as possible
            client.SetAttribute ("PacketSize", UintegerValue (payloadSize));
            clientApp = client.Install (wifiStaNode.Get (0));
            clientApp.Start (Seconds (startTime));
            clientApp.Stop (Seconds (stopTime));
          }
          else // TCP
          {
            uint16_t sinkPort = port;
            Address sinkAddress (InetSocketAddress (apInterface.GetAddress (0), sinkPort));
            PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", sinkAddress);
            serverApp = sinkHelper.Install (wifiApNode.Get (0));
            serverApp.Start (Seconds (startTime));
            serverApp.Stop (Seconds (stopTime));

            OnOffHelper client ("ns3::TcpSocketFactory", sinkAddress);
            client.SetAttribute ("PacketSize", UintegerValue (payloadSize));
            client.SetAttribute ("DataRate", DataRateValue (DataRate ("1Gbps")));
            clientApp = client.Install (wifiStaNode.Get (0));
            clientApp.Start (Seconds (startTime));
            clientApp.Stop (Seconds (stopTime));
          }

          Simulator::Stop (Seconds (stopTime + 1));

          // Run simulation
          Simulator::Run ();

          // Calculate Goodput
          double goodput = 0.0;
          if (transport == "UDP")
          {
            Ptr<UdpServer> server = DynamicCast<UdpServer> (serverApp.Get (0));
            uint64_t totalBytes = server->GetReceived ();
            goodput = (totalBytes * 8.0) / (stopTime - startTime) / 1e6; // Mbps
          }
          else
          {
            Ptr<PacketSink> sink = DynamicCast<PacketSink> (serverApp.Get (0));
            uint64_t totalBytes = sink->GetTotalRx ();
            goodput = (totalBytes * 8.0) / (stopTime - startTime) / 1e6; // Mbps
          }

          std::cout << channelWidthNames[cwi] << ","
                    << (shortGi ? "short" : "long") << ","
                    << (rtsCts ? "rtscts" : "nortscts") << ","
                    << unsigned(mcs) << ","
                    << goodput << std::endl;

          Simulator::Destroy ();
        }
      }
    }
  }

  return 0;
}