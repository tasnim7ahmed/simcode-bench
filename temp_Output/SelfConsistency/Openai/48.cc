/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Simple IEEE 802.11n Simulation: PHY model comparison, parameter sweep, and logging
 * Supports: SpectrumWifiPhy and YansWifiPhy, UDP/TCP traffic, packet captures, logging SNR
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/spectrum-wifi-helper.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/ipv4-global-routing-helper.h"
#include <iostream>
#include <iomanip>
#include <vector>
#include <string>

using namespace ns3;

// Struct to hold simulation parameters
struct SimParams
{
  Time simTime = Seconds(10.0);
  double distance = 10.0;
  std::string phyModel = "Yans"; // "Spectrum" or "Yans"
  bool isUdp = true;
  bool enablePcap = false;
  std::vector<uint8_t> mcsList = {0, 3, 7}; // Example MCS indices
  std::vector<uint16_t> channelWidths = {20, 40}; // MHz
  std::vector<bool> guardIntervals = {false, true}; // false=long, true=short
};

// Global containers for logging
std::vector<double> g_receivedSignalPowers_dBm;
std::vector<double> g_noisePowers_dBm;

// Callbacks for SNR, Signal, Noise logging
void
SnrTrace (std::string context, double snr)
{
  // Optionally log SNR if needed
  // std::cout << "SNR: " << snr << " dB " << context << std::endl;
}

void
SignalNoiseTrace (std::string context, double rxPower, double noisePower)
{
  g_receivedSignalPowers_dBm.push_back (10*log10(rxPower) + 30); // W -> dBm
  g_noisePowers_dBm.push_back (10*log10(noisePower) + 30);       // W -> dBm
}

void
RxEvent (Ptr<const Packet> packet, const Address &)
{
  // Placeholder for logging receive events
}

void
PrintInstructions ()
{
  std::cout << "\nIEEE 802.11n PHY comparison simulation (ns-3)\n";
  std::cout << "Options: \n";
  std::cout << "    --phyModel=[Yans|Spectrum]      (default: Yans)\n";
  std::cout << "    --isUdp=[1|0]                   (default: 1, set 0 for TCP)\n";
  std::cout << "    --enablePcap=1                  (default: 0)\n";
  std::cout << "    --distance=VALUE                (default: 10.0 m)\n";
  std::cout << "    --simTime=VALUE                 (seconds, default: 10)\n";
  std::cout << "    --mcsList=idx1,idx2,...         (default: 0,3,7)\n";
  std::cout << "    --channelWidths=w1,w2,...       (MHz, default: 20,40)\n";
  std::cout << "    --guardIntervals=0,1,...        (0:long, 1:short, default: 0,1)\n";
  std::cout << "\n";
}

// Helper to parse CSV list -- e.g., for command-line vector params
template <typename T>
std::vector<T>
ParseCsvList (std::string csvStr)
{
  std::vector<T> list;
  std::istringstream iss (csvStr);
  std::string item;
  while (std::getline (iss, item, ','))
    {
      std::istringstream ss (item);
      T val;
      ss >> val;
      if (ss)
        list.push_back (val);
    }
  return list;
}

int
main (int argc, char *argv[])
{
  SimParams params;

  // Parse command-line
  std::string mcsListStr = "0,3,7";
  std::string chwListStr = "20,40";
  std::string giListStr  = "0,1";

  CommandLine cmd;
  cmd.AddValue ("simTime", "Simulation time (seconds)", params.simTime);
  cmd.AddValue ("distance", "Distance between STA and AP (meters)", params.distance);
  cmd.AddValue ("phyModel", "Wi-Fi PHY Model: Yans or Spectrum", params.phyModel);
  cmd.AddValue ("isUdp",   "UDP (1) or TCP (0) traffic", params.isUdp);
  cmd.AddValue ("enablePcap", "Enable/disable pcap tracing", params.enablePcap);
  cmd.AddValue ("mcsList", "Comma-separated 802.11n MCS indices", mcsListStr);
  cmd.AddValue ("channelWidths", "Comma-separated channel widths (MHz)", chwListStr);
  cmd.AddValue ("guardIntervals", "Comma-separated guard interval: 0=long,1=short", giListStr);
  cmd.AddValue ("help", "Print this help message", false);

  bool help = false;
  cmd.Parse (argc, argv);

  params.mcsList = ParseCsvList<uint8_t> (mcsListStr);
  params.channelWidths = ParseCsvList<uint16_t> (chwListStr);
  params.guardIntervals.clear ();
  for (auto s : ParseCsvList<uint8_t> (giListStr))
    params.guardIntervals.push_back (s != 0);

  if (help)
    {
      PrintInstructions ();
      return 0;
    }

  PrintInstructions ();

  std::cout << "SimTime: " << params.simTime.GetSeconds () << " s   Distance: " << params.distance << " m\n";
  std::cout << "PHY: " << params.phyModel << "   Traffic: " << (params.isUdp ? "UDP" : "TCP") << "   PCAP: " << params.enablePcap << "\n";
  std::cout << "MCS: ";
  for (auto m : params.mcsList) std::cout << unsigned(m) << " ";
  std::cout << "\nChannel widths: ";
  for (auto w : params.channelWidths) std::cout << w << " ";
  std::cout << "\nGuard Intervals: ";
  for (auto b : params.guardIntervals) std::cout << (b ? "Short " : "Long ");
  std::cout << "\n\n";

  // Run parameter sweeps
  std::cout << "MCS\tChWidth\tGI\tThroughput(Mbps)\tAvg.Signal(dBm)\tAvg.Noise(dBm)\tAvg.SNR(dB)\n";
  for (uint8_t mcs : params.mcsList)
    {
      for (uint16_t chw : params.channelWidths)
        {
          for (bool shortGi : params.guardIntervals)
            {
              g_receivedSignalPowers_dBm.clear ();
              g_noisePowers_dBm.clear ();

              NodeContainer wifiStaNode, wifiApNode;
              wifiStaNode.Create (1);
              wifiApNode.Create (1);

              // Mobility
              MobilityHelper mobility;
              Ptr<ListPositionAllocator> pos = CreateObject<ListPositionAllocator> ();
              pos->Add (Vector (0.0, 0.0, 0.0));                      // AP
              pos->Add (Vector (params.distance, 0.0, 0.0));           // STA
              mobility.SetPositionAllocator (pos);
              mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
              mobility.Install (wifiApNode);
              mobility.Install (wifiStaNode);

              // WiFi helpers
              WifiHelper wifi;
              wifi.SetStandard (WIFI_STANDARD_80211n_5GHZ);

              // Use explicit MCS
              std::ostringstream dsss;
              dsss << "HtMcs" << unsigned(mcs);
              wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                            "DataMode", StringValue (dsss.str ()),
                                            "ControlMode", StringValue ("HtMcs0"));

              NetDeviceContainer staDevice, apDevice;
              Ptr<WifiPhy> phy;
              Ptr<SpectrumWifiPhy> spectrumPhy;
              Ptr<YansWifiPhy> yansPhy;

              if (params.phyModel == "Spectrum" || params.phyModel == "spectrum")
                {
                  SpectrumWifiPhyHelper spectrumPhyHelper = SpectrumWifiPhyHelper::Default ();
                  Ptr<SpectrumChannel> spectrumChannel = CreateObject<MultiModelSpectrumChannel> ();
                  spectrumPhyHelper.SetChannel (spectrumChannel);
                  spectrumPhyHelper.Set ("ChannelWidth", UintegerValue (chw));
                  spectrumPhyHelper.Set ("ShortGuardEnabled", BooleanValue (shortGi));
                  spectrumPhyHelper.SetErrorRateModel ("ns3::NistErrorRateModel");
                  WifiMacHelper mac;
                  mac.SetType ("ns3::StaWifiMac", "Ssid", SsidValue (Ssid ("ns3-80211n")));
                  staDevice = wifi.Install (spectrumPhyHelper, mac, wifiStaNode);
                  mac.SetType ("ns3::ApWifiMac", "Ssid", SsidValue (Ssid ("ns3-80211n")));
                  apDevice = wifi.Install (spectrumPhyHelper, mac, wifiApNode);
                  spectrumPhy = DynamicCast<SpectrumWifiPhy> (staDevice.Get (0)->GetObject<WifiNetDevice>()->GetPhy ());
                  // Connect Traces
                  Config::Connect ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/$ns3::SpectrumWifiPhy/MonitorSnifferRx", MakeCallback (&RxEvent));
                  Config::Connect ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/$ns3::SpectrumWifiPhy/State/State", MakeCallback (&SnrTrace));
                  Config::Connect ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/$ns3::SpectrumWifiPhy/MonitorSnifferRx", MakeCallback (&RxEvent));
                  Config::Connect ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/$ns3::SpectrumWifiPhy/MonitorSnifferRx", MakeCallback (&RxEvent));
                }
              else
                {
                  YansWifiPhyHelper phyHelper = YansWifiPhyHelper::Default ();
                  YansWifiChannelHelper channelHelper = YansWifiChannelHelper::Default ();
                  phyHelper.SetChannel (channelHelper.Create ());
                  phyHelper.Set ("ChannelWidth", UintegerValue (chw));
                  phyHelper.Set ("ShortGuardEnabled", BooleanValue (shortGi));
                  phyHelper.SetErrorRateModel ("ns3::NistErrorRateModel");
                  WifiMacHelper mac;
                  mac.SetType ("ns3::StaWifiMac", "Ssid", SsidValue (Ssid ("ns3-80211n")));
                  staDevice = wifi.Install (phyHelper, mac, wifiStaNode);
                  mac.SetType ("ns3::ApWifiMac", "Ssid", SsidValue (Ssid ("ns3-80211n")));
                  apDevice = wifi.Install (phyHelper, mac, wifiApNode);
                  yansPhy = DynamicCast<YansWifiPhy> (staDevice.Get (0)->GetObject<WifiNetDevice>()->GetPhy ());
                  // Trace signal/noise for logging
                  std::string path = "/NodeList/" + std::to_string (wifiStaNode.Get (0)->GetId ())
                    + "/DeviceList/0/$ns3::WifiNetDevice/Phy/$ns3::YansWifiPhy/State/RxOk";
                  Config::Connect ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/$ns3::YansWifiPhy/MonitorSnifferRx", MakeCallback (&RxEvent));
                  Config::Connect ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/$ns3::YansWifiPhy/MonitorSnifferRx", MakeCallback (&RxEvent));
                  // To get signal/noise values:
                  yansPhy->TraceConnectWithoutContext ("MonitorSnifferRx", MakeCallback (
                    [](Ptr<const Packet> pkt, uint16_t freq, WifiTxVector txVector, MpduInfo a, SignalNoiseDbm sn)
                    {
                      g_receivedSignalPowers_dBm.push_back (sn.signal);
                      g_noisePowers_dBm.push_back (sn.noise);
                    }));
                }

              // Internet and IP config
              InternetStackHelper stack;
              stack.Install (wifiApNode);
              stack.Install (wifiStaNode);

              Ipv4AddressHelper address;
              address.SetBase ("192.168.1.0", "255.255.255.0");
              Ipv4InterfaceContainer apInterface = address.Assign (apDevice);
              Ipv4InterfaceContainer staInterface = address.Assign (staDevice);
              Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

              // Application: UDP or TCP
              uint16_t port = 9;
              ApplicationContainer sinkApp, clientApp;
              if (params.isUdp)
                {
                  PacketSinkHelper sinkHelper ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port));
                  sinkApp = sinkHelper.Install (wifiApNode.Get (0));
                  sinkApp.Start (Seconds (0.0));
                  sinkApp.Stop (params.simTime);

                  OnOffHelper onoff ("ns3::UdpSocketFactory", InetSocketAddress (apInterface.GetAddress (0), port));
                  onoff.SetAttribute ("DataRate", DataRateValue (DataRate ("200Mbps")));
                  onoff.SetAttribute ("PacketSize", UintegerValue (1472));
                  onoff.SetAttribute ("StartTime", TimeValue (Seconds (1.0)));
                  onoff.SetAttribute ("StopTime",  TimeValue (params.simTime));
                  clientApp.Add (onoff.Install (wifiStaNode.Get (0)));
                }
              else
                {
                  PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port));
                  sinkApp = sinkHelper.Install (wifiApNode.Get (0));
                  sinkApp.Start (Seconds (0.0));
                  sinkApp.Stop (params.simTime);

                  // BulkSendApp for TCP
                  BulkSendHelper bulk ("ns3::TcpSocketFactory", InetSocketAddress (apInterface.GetAddress (0), port));
                  bulk.SetAttribute ("MaxBytes", UintegerValue (0u));
                  bulk.SetAttribute ("SendSize", UintegerValue (1472));
                  clientApp = bulk.Install (wifiStaNode.Get (0));
                  clientApp.Start (Seconds (1.0));
                  clientApp.Stop (params.simTime);
                }

              // Enable pcap if required
              if (params.enablePcap)
                {
                  if (params.phyModel == "Spectrum" || params.phyModel == "spectrum")
                    {
                      SpectrumWifiPhyHelper::Default ().EnablePcap ("wifi-spectrum-ap", apDevice);
                      SpectrumWifiPhyHelper::Default ().EnablePcap ("wifi-spectrum-sta", staDevice);
                    }
                  else
                    {
                      YansWifiPhyHelper::Default ().EnablePcap ("wifi-yans-ap", apDevice);
                      YansWifiPhyHelper::Default ().EnablePcap ("wifi-yans-sta", staDevice);
                    }
                }

              // Run the simulation
              Simulator::Stop (params.simTime + Seconds (1.0));
              Simulator::Run ();

              // Throughput calculation
              uint64_t totalRx = DynamicCast<PacketSink> (sinkApp.Get (0))->GetTotalRx ();
              double throughputMbps = totalRx * 8.0 / (params.simTime.GetSeconds () * 1e6);

              // Compute signal/noise/SNR stats (averaged over received packets)
              double meanSignal = 0, meanNoise = 0, meanSnr = 0;
              size_t count = g_receivedSignalPowers_dBm.size ();
              for (size_t i = 0; i < count; ++i)
                {
                  meanSignal += g_receivedSignalPowers_dBm[i];
                  meanNoise  += g_noisePowers_dBm[i];
                }
              if (count > 0)
                {
                  meanSignal /= count;
                  meanNoise /= count;
                  meanSnr = meanSignal - meanNoise;
                }

              // Output results for this configuration
              std::cout << unsigned(mcs) << "\t"
                        << chw     << "\t"
                        << (shortGi ? "Short" : "Long") << "\t"
                        << std::fixed << std::setprecision (4) << throughputMbps << "\t\t"
                        << std::setprecision (2) << meanSignal << "\t\t"
                        << meanNoise << "\t\t"
                        << meanSnr << "\n";

              Simulator::Destroy ();
            }
        }
    }

  std::cout << "\nSimulation done.\n";
  std::cout << "To toggle between UDP and TCP: Pass --isUdp=1 or --isUdp=0\n";
  std::cout << "To toggle PHY type:           Pass --phyModel=Yans or --phyModel=Spectrum\n";
  std::cout << "To capture packets:           Pass --enablePcap=1\n";
  std::cout << "To select MCS/channel/GI:     Pass --mcsList=... --channelWidths=... --guardIntervals=...\n\n";
  return 0;
}