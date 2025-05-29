#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/config-store-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include <vector>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <map>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("Wifi7MscThroughputTest");

struct SimParams
{
  std::vector<uint8_t> mcsList;
  std::vector<uint16_t> channelWidthList;
  std::vector<std::string> giList; // ns, e.g., "3200", "1600", "800" (as string for compatibility)
  std::vector<double> freqList;    // GHz: 2.4, 5, 6
  bool uplinkOfdma;
  bool bsprEnabled; // BSRP flag
  uint32_t nSta;
  std::string trafficType; // "TCP" or "UDP"
  uint32_t payloadSize;
  uint32_t mpduBufferSize;
  std::string phySettings;
  double simTime;
  double tolerance;
};

std::map<uint8_t, double> mcsMaxThroughput = {
    // These are example values in Mbps, real values depend on channel width, guard interval
    {0,  100},
    {1,  200},
    {2,  300},
    {3,  400},
    {4,  500},
    {5,  600},
    {6,  800},
    {7, 1000},
    {8, 1200},
    {9, 1400},
    {10,1600},
    {11,1800},
    {12,2000},
    {13,2200}
};

struct ResultRow
{
  uint8_t mcs;
  uint16_t channelWidth;
  std::string gi;
  double throughputMbps;
};

std::string
DoubleToStr (double val, int precision = 2)
{
  std::ostringstream oss;
  oss << std::fixed << std::setprecision (precision) << val;
  return oss.str ();
}

void
PrintTable (std::vector<ResultRow> &rows)
{
  std::cout << std::setw (6) << "MCS"
            << std::setw (12) << "ChWidth"
            << std::setw (10) << "GI(ns)"
            << std::setw (14) << "Throughput(Mbps)" << std::endl;

  std::cout << std::string (44, '-') << std::endl;
  for (const auto &r : rows)
    {
      std::cout << std::setw (6) << unsigned (r.mcs)
                << std::setw (12) << r.channelWidth
                << std::setw (10) << r.gi
                << std::setw (14) << DoubleToStr (r.throughputMbps) << std::endl;
    }
}

void
ParseInputParams (int argc, char *argv[], SimParams &params)
{
  std::string mcsStr = "10";        // Default random value
  std::string channelWidthStr = "20";
  std::string giStr = "800";      // ns
  std::string freqStr = "2.4";
  params.trafficType = "UDP";
  params.payloadSize = 1472;
  params.nSta = 4;
  params.simTime = 10.0;
  params.uplinkOfdma = 0;
  params.bsprEnabled = 0;
  params.mpduBufferSize = 64;
  params.phySettings = "";
  params.tolerance = 0.25; // 25%
  CommandLine cmd;
  cmd.AddValue ("mcs", "Comma-separated list of MCS values", mcsStr);
  cmd.AddValue ("channelWidth", "Comma-separated channel widths", channelWidthStr);
  cmd.AddValue ("gi", "Comma-separated GI values (800,1600,3200 ns)", giStr);
  cmd.AddValue ("freq", "Comma-separated frequencies (2.4,5,6)", freqStr);
  cmd.AddValue ("nSta", "Number of Stations", params.nSta);
  cmd.AddValue ("trafficType", "TCP or UDP", params.trafficType);
  cmd.AddValue ("payloadSize", "Payload Size in bytes", params.payloadSize);
  cmd.AddValue ("mpduBufferSize", "MPDU buffer size", params.mpduBufferSize);
  cmd.AddValue ("uplinkOfdma", "Enable Uplink OFDMA [0/1]", params.uplinkOfdma);
  cmd.AddValue ("bsprEnabled", "Enable BSRP [0/1]", params.bsprEnabled);
  cmd.AddValue ("simTime", "Simulation Time (s)", params.simTime);
  cmd.AddValue ("tolerance", "Throughput tolerance (fraction)", params.tolerance);
  cmd.Parse (argc, argv);

  auto split = [] (const std::string &s, char delim) {
    std::vector<std::string> elems;
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, delim))
      elems.push_back(item);
    return elems;
  };

  for (const std::string &tok : split (mcsStr, ','))
    params.mcsList.push_back (stoi (tok));
  for (const std::string &tok : split (channelWidthStr, ','))
    params.channelWidthList.push_back (stoi (tok));
  for (const std::string &tok : split (giStr, ','))
    params.giList.push_back (tok);
  for (const std::string &tok : split (freqStr, ','))
    params.freqList.push_back (stod (tok));
}

std::string
GetPhyModeString (double freq, uint16_t chWidth)
{
  // Just a label, can be extended for debugging.
  std::ostringstream oss;
  oss << (freq < 3 ? "2.4GHz" : (freq < 6 ? "5GHz" : "6GHz")) << "_" << chWidth;
  return oss.str ();
}

uint16_t
GetChannelNumber (double freq)
{
  if (std::fabs (freq - 2.4) < 1e-3)
    return 1;
  if (std::fabs (freq - 5.0) < 1e-3)
    return 36;
  if (std::fabs (freq - 6.0) < 1e-3)
    return 1;
  return 36;
}

uint16_t
GetCenterFrequency (double freq, uint16_t width)
{
  // For simplicity, follow Wi-Fi standard center freq for 2.4G (e.g., 2412), 5G (e.g., 5180), 6G (5955+)
  if (std::fabs (freq - 2.4) < 1e-3)
    return 2412;
  if (std::fabs (freq - 5.0) < 1e-3)
    return 5180;
  if (std::fabs (freq - 6.0) < 1e-3)
    return 5955;
  return 5180;
}

uint32_t
GetGiNs (const std::string &gi)
{
  if (gi == "3200") return 3200;
  if (gi == "1600") return 1600;
  return 800;
}

void
SetWifiStandard (Ptr<WifiPhy> phy, double freq)
{
  if (std::fabs (freq - 2.4) < 1e-3)
    phy->SetChannelNumber (1);
  // For 5 and 6 GHz Wi-Fi 7
  phy->SetAttribute ("Frequency", UintegerValue (GetCenterFrequency (freq, 20)));
}

bool
IsThroughputValid(double measured, double minExpect, double maxExpect, double tolerance)
{
  double minT = minExpect * (1.0-tolerance);
  double maxT = maxExpect * (1.0+tolerance);
  return (measured >= minT && measured <= maxT);
}

int main (int argc, char *argv[])
{
  SimParams params;
  ParseInputParams (argc, argv, params);

  std::vector<ResultRow> results;
  bool globalError = false;

  for (double freq : params.freqList)
    for (uint16_t chWidth : params.channelWidthList)
      for (const std::string &gi : params.giList)
        for (uint8_t mcs : params.mcsList)
          {
            uint32_t nSta = params.nSta;
            uint32_t payloadSize = params.payloadSize;

            NodeContainer ap, stas;
            ap.Create (1);
            stas.Create (nSta);

            WifiHelper wifi;
            wifi.SetStandard (WIFI_STANDARD_80211be);

            YansWifiPhyHelper phy;
            YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
            phy.SetChannel (channel.Create ());
            phy.Set ("ChannelWidth", UintegerValue (chWidth));
            phy.Set ("Frequency", UintegerValue (GetCenterFrequency (freq, chWidth)));
            phy.Set ("GuardInterval", TimeValue (NanoSeconds(GetGiNs (gi))));
            phy.Set ("ShortGuardEnabled", BooleanValue (GetGiNs(gi) != 800 ? true : false));

            // BSRP, OFDMA relevant attributes...
            phy.Set ("BsrEnabled", BooleanValue (params.bsprEnabled));
            // Not full OFDMA model in NS-3.41, but set attributes if present.
            phy.Set ("UplinkOfdmaEnabled", BooleanValue (params.uplinkOfdma));

            WifiMacHelper mac;
            Ssid ssid = Ssid ("wifi7-sid");

            mac.SetType ("ns3::StaWifiMac",
                         "Ssid", SsidValue (ssid),
                         "QosSupported", BooleanValue (true));
            NetDeviceContainer staDevs = wifi.Install (phy, mac, stas);

            mac.SetType ("ns3::ApWifiMac",
                         "Ssid", SsidValue (ssid),
                         "QosSupported", BooleanValue (true));
            NetDeviceContainer apDevs = wifi.Install (phy, mac, ap);

            // MCS configuration
            for (uint32_t i = 0; i < staDevs.GetN (); ++i)
              {
                Ptr<WifiNetDevice> dev = DynamicCast<WifiNetDevice> (staDevs.Get (i));
                Ptr<HePhy> hePhy = DynamicCast<HePhy> (dev->GetPhy ());
                if (hePhy)
                  {
                    hePhy->SetAttribute ("McsValue", UintegerValue (mcs));
                  }
              }
            for (uint32_t i = 0; i < apDevs.GetN (); ++i)
              {
                Ptr<WifiNetDevice> dev = DynamicCast<WifiNetDevice> (apDevs.Get (i));
                Ptr<HePhy> hePhy = DynamicCast<HePhy> (dev->GetPhy ());
                if (hePhy)
                  {
                    hePhy->SetAttribute ("McsValue", UintegerValue (mcs));
                  }
              }

            MobilityHelper mobility;
            mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
            mobility.Install (ap);
            mobility.Install (stas);

            InternetStackHelper internet;
            internet.Install (ap);
            internet.Install (stas);

            Ipv4AddressHelper ipv4;
            ipv4.SetBase ("192.168.1.0", "255.255.255.0");
            Ipv4InterfaceContainer intfSta = ipv4.Assign (staDevs);
            Ipv4InterfaceContainer intfAp = ipv4.Assign (apDevs);

            // MPDU buffer size (for 802.11be, if simulating aggregated frames)
            Config::SetDefault ("ns3::WifiMacQueue::MaxSize", StringValue (std::to_string(params.mpduBufferSize) + "p"));

            ApplicationContainer serverApps, clientApps;
            uint16_t port = 5000;
            double appStart = 1.0;
            double appStop = params.simTime - 1.0;
            if (params.trafficType == "TCP")
              {
                for (uint32_t i = 0; i < nSta; ++i)
                  {
                    // TCP server on AP, client on STA
                    Address bindAddress (InetSocketAddress (Ipv4Address::GetAny (), port + i));
                    PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", bindAddress);
                    ApplicationContainer server = sinkHelper.Install (ap);
                    serverApps.Add (server);

                    OnOffHelper client ("ns3::TcpSocketFactory",
                                        Address (InetSocketAddress (intfAp.GetAddress (0), port + i)));
                    client.SetAttribute ("PacketSize", UintegerValue (payloadSize));
                    client.SetAttribute ("DataRate", StringValue ("1Gbps"));
                    client.SetAttribute ("StartTime", TimeValue (Seconds (appStart)));
                    client.SetAttribute ("StopTime", TimeValue (Seconds (appStop)));
                    client.SetAttribute ("MaxBytes", UintegerValue (0));
                    ApplicationContainer clientApp = client.Install (stas.Get (i));
                    clientApps.Add (clientApp);
                  }
              }
            else // UDP
              {
                for (uint32_t i = 0; i < nSta; ++i)
                  {
                    UdpServerHelper server (port + i);
                    serverApps.Add (server.Install (ap));
                    UdpClientHelper client (intfAp.GetAddress (0), port + i);
                    client.SetAttribute ("MaxPackets", UintegerValue (0));
                    client.SetAttribute ("PacketSize", UintegerValue (payloadSize));
                    client.SetAttribute ("Interval", TimeValue (Seconds (0.0001)));
                    clientApps.Add (client.Install (stas.Get (i)));
                  }
              }

            serverApps.Start (Seconds (appStart));
            serverApps.Stop (Seconds (appStop));
            clientApps.Start (Seconds (appStart));
            clientApps.Stop (Seconds (appStop + 1));

            FlowMonitorHelper flowmon;
            Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

            Simulator::Stop (Seconds (params.simTime));
            Simulator::Run ();

            // Gather cumulative RX bytes from all server apps (on AP)
            double totalRxBytes = 0;
            if (params.trafficType == "TCP")
              {
                for (uint32_t i = 0; i < serverApps.GetN (); ++i)
                  {
                    Ptr<PacketSink> sink = DynamicCast<PacketSink> (serverApps.Get (i));
                    totalRxBytes += sink ? sink->GetTotalRx () : 0;
                  }
              }
            else
              {
                for (uint32_t i = 0; i < serverApps.GetN (); ++i)
                  {
                    Ptr<UdpServer> udp = DynamicCast<UdpServer> (serverApps.Get (i));
                    if (udp)
                      totalRxBytes += udp->GetReceived () * payloadSize;
                  }
              }
            double duration = appStop - appStart;
            double throughputMbps = (totalRxBytes * 8.0) / (duration * 1e6);

            // Estimate expected throughput: channelWidth * efficiency * MCS scaling
            double eff = 0.9; // overall efficiency
            double mcsBase = 500.0 + mcs * 100.0; // Example: Needs real calculations for 802.11be
            double minExpect = (chWidth * eff) * (mcs + 1); // crude lower bound estimation, placeholder
            double maxExpect = mcsMaxThroughput.count(mcs) ? mcsMaxThroughput[mcs] : (chWidth * 20);
            bool valid = IsThroughputValid(throughputMbps, minExpect, maxExpect, params.tolerance);

            results.push_back ({mcs, chWidth, gi, throughputMbps});
            if (!valid)
              {
                NS_LOG_ERROR ("Throughput validation failed for MCS=" << unsigned(mcs)
                            << " CW=" << chWidth << " GI=" << gi
                            << ": computed=" << throughputMbps << " Mbps, expected=["
                            << minExpect << "," << maxExpect << "] x tolerance " << params.tolerance);
                globalError = true;
              }

            Simulator::Destroy ();
            if (globalError)
              {
                std::cerr << "**ERROR: Aborting due to throughput validation**" << std::endl;
                exit (1);
              }
          }

  PrintTable (results);
  return 0;
}