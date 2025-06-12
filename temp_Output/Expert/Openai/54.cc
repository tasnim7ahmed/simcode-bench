#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/spectrum-wifi-helper.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/ssid.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/ipv4-global-routing-helper.h"
#include <vector>
#include <string>
#include <iomanip>
#include <sstream>

using namespace ns3;

void
PrintGoodput (std::string label, uint64_t bytes, double duration)
{
  double goodputMbps = (bytes * 8.0) / (duration * 1e6);
  std::cout << label << "  Goodput: " << std::fixed << std::setprecision(4)
            << goodputMbps << " Mbps" << std::endl;
}

int
main (int argc, char *argv[])
{
  // User-customizable parameters
  double simulationTime = 10.0;          // seconds
  double distance = 5.0;                 // meters
  std::string trafficType = "UDP";       // "TCP" or "UDP"
  uint16_t packetSize = 1472;            // bytes
  bool printHeader = true;

  CommandLine cmd;
  cmd.AddValue ("simulationTime", "Duration of simulation in seconds", simulationTime);
  cmd.AddValue ("distance", "STA-AP distance in meters", distance);
  cmd.AddValue ("trafficType", "UDP or TCP", trafficType);
  cmd.AddValue ("packetSize", "Packet size in bytes", packetSize);
  cmd.Parse (argc, argv);

  std::vector<std::string> rtsCts = { "No", "Yes" };
  std::vector<uint32_t> channelWidths = { 20, 40, 80, 160 };
  std::vector<std::string> guardIntervals = { "LongGuardInterval", "ShortGuardInterval" };
  std::vector<uint8_t> mcsLevels = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
  std::vector<std::string> bw_modes = { "160", "80+80" }; // 802.11ac allows both modes at 160 MHz

  if (printHeader)
  {
    std::cout << "TrafficType,RTS_CTS,ChannelWidthMHz,BWMode,MCS,GuardInterval,Goodput_Mbps" << std::endl;
  }

  for (const auto &rts : rtsCts)
  {
    for (const auto &cw : channelWidths)
    {
      std::vector<std::string> useBWmodes;
      if (cw != 160) useBWmodes = { std::to_string (cw) };
      else           useBWmodes = bw_modes; // For 160, test both 160 MHz and 80+80 MHz modes

      for (const auto &bwMode : useBWmodes)
      {
        for (const auto &mcs : mcsLevels)
        {
          for (const auto &gi : guardIntervals)
          {
            SeedManager::SetSeed (12345);

            NodeContainer wifiStaNode, wifiApNode;
            wifiStaNode.Create (1);
            wifiApNode.Create (1);

            YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
            Ptr<YansWifiChannel> chanPtr = channel.Create ();

            SpectrumWifiPhyHelper phy;
            phy.SetChannel (chanPtr);
            phy.Set ("ShortGuardEnabled", BooleanValue (gi == "ShortGuardInterval"));

            WifiHelper wifi;
            wifi.SetStandard (WIFI_STANDARD_80211ac);

            WifiMacHelper mac;

            // Channel width and HT/VHT operation
            phy.Set ("ChannelWidth", UintegerValue (cw));
            if (cw == 160 && bwMode == "80+80")
            {
              phy.Set ("Frequency", UintegerValue (5180)); // center freq for 80+80
              phy.Set ("ChannelSettings", StringValue ("[{Num=1,Width=80},{Num=1,Width=80}]"));
            }
            else
            {
              phy.Set ("Frequency", UintegerValue (5180)); // Channel 36 (primary 5180 MHz)
            }

            // Disable PCAP and Radiotap output for performance
            // Set RTS/CTS
            wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                         "DataMode", StringValue ("VhtMcs" + std::to_string(mcs)),
                                         "ControlMode", StringValue ("VhtMcs0"),
                                         "RtsCtsThreshold", UintegerValue (rts == "Yes" ? 0 : 9999));

            Ssid ssid = Ssid ("ns3-80211ac");

            mac.SetType ("ns3::StaWifiMac",
                         "Ssid", SsidValue (ssid),
                         "ActiveProbing", BooleanValue (false));
            NetDeviceContainer staDevice = wifi.Install (phy, mac, wifiStaNode);

            mac.SetType ("ns3::ApWifiMac",
                         "Ssid", SsidValue (ssid));
            NetDeviceContainer apDevice = wifi.Install (phy, mac, wifiApNode);

            MobilityHelper mobility;
            mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
            mobility.Install (wifiApNode);
            mobility.Install (wifiStaNode);
            Ptr<MobilityModel> apMobility = wifiApNode.Get (0)->GetObject<MobilityModel> ();
            apMobility->SetPosition (Vector (0.0, 0.0, 0.0));
            Ptr<MobilityModel> staMobility = wifiStaNode.Get (0)->GetObject<MobilityModel> ();
            staMobility->SetPosition (Vector (distance, 0.0, 0.0));

            InternetStackHelper stack;
            stack.Install (wifiApNode);
            stack.Install (wifiStaNode);

            Ipv4AddressHelper address;
            address.SetBase ("10.1.1.0", "255.255.255.0");

            Ipv4InterfaceContainer staIf, apIf;
            staIf = address.Assign (staDevice);
            address.NewNetwork ();
            apIf = address.Assign (apDevice);

            uint16_t port = 9;
            ApplicationContainer clientApps, serverApps;

            if (trafficType == "UDP")
            {
              UdpServerHelper server (port);
              serverApps = server.Install (wifiApNode.Get (0));
              serverApps.Start (Seconds (0.0));
              serverApps.Stop (Seconds (simulationTime + 1.0));

              UdpClientHelper client (apIf.GetAddress (0), port);
              client.SetAttribute ("MaxPackets", UintegerValue (4294967295u));
              client.SetAttribute ("Interval", TimeValue (Seconds (0.0)));
              client.SetAttribute ("PacketSize", UintegerValue (packetSize));
              clientApps.Add (client.Install (wifiStaNode.Get (0)));
              clientApps.Start (Seconds (1.0));
              clientApps.Stop (Seconds (simulationTime + 1.0));
            }
            else // TCP
            {
              Address sinkLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), port));
              PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", sinkLocalAddress);
              serverApps = sinkHelper.Install (wifiApNode.Get (0));
              serverApps.Start (Seconds (0.0));
              serverApps.Stop (Seconds (simulationTime + 1.0));

              OnOffHelper clientHelper ("ns3::TcpSocketFactory", InetSocketAddress (apIf.GetAddress (0), port));
              clientHelper.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
              clientHelper.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
              clientHelper.SetAttribute ("PacketSize", UintegerValue (packetSize));
              clientHelper.SetAttribute ("DataRate", DataRateValue (DataRate ("10Gbps")));
              clientApps.Add (clientHelper.Install (wifiStaNode.Get (0)));
              clientApps.Start (Seconds (1.0));
              clientApps.Stop (Seconds (simulationTime + 1.0));
            }

            Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

            Simulator::Stop (Seconds (simulationTime + 1.0));
            Simulator::Run ();

            uint64_t totalRx = 0;
            double   duration = simulationTime;
            if (trafficType == "UDP")
            {
              Ptr<UdpServer> udpServer = DynamicCast<UdpServer> (serverApps.Get (0));
              totalRx = udpServer->GetReceived () * packetSize;
            }
            else
            {
              Ptr<PacketSink> tcpSink = DynamicCast<PacketSink> (serverApps.Get (0));
              totalRx = tcpSink->GetTotalRx ();
            }

            std::ostringstream oss;
            oss << trafficType << ",";
            oss << rts << ",";
            oss << cw << ",";
            oss << bwMode << ",";
            oss << unsigned (mcs) << ",";
            oss << (gi == "ShortGuardInterval" ? "Short" : "Long") << ",";
            oss << std::fixed << std::setprecision(4) << (totalRx * 8.0 / (duration * 1e6));
            std::cout << oss.str() << std::endl;

            Simulator::Destroy ();
          }
        }
      }
    }
  }

  return 0;
}