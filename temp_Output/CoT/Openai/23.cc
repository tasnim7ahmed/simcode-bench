#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include <iomanip>
#include <vector>
#include <sstream>
#include <map>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("Wifi7ThroughputEvaluation");

struct SimResult
{
  uint8_t mcs;
  uint16_t channelWidth;
  std::string gi;
  double throughputMbps;
  bool valid;
};

void
CheckThroughput (double throughput, double min, double max, std::ostringstream &oss, uint8_t mcs, uint16_t width, std::string gi, bool &errorFound)
{
  if (throughput < min || throughput > max)
    {
      oss << "ERROR: Throughput out of expected bounds for MCS " << unsigned(mcs)
          << ", width " << width << ", GI " << gi << ": " << throughput << " Mbps\n";
      errorFound = true;
    }
}

std::string
GiToString (Time gi)
{
  if (gi == NanoSeconds (800))
    return "0.8us";
  else if (gi == NanoSeconds (1600))
    return "1.6us";
  else if (gi == NanoSeconds (3200))
    return "3.2us";
  else
    {
      std::ostringstream oss;
      oss << gi.GetNanoSeconds ()/1000 << "ns";
      return oss.str();
    }
}

int main (int argc, char *argv[])
{
  // Default parameters
  uint8_t mcs = 7;
  std::string freqBand = "5";
  bool uplinkOfdma = false;
  bool bspr = false;
  uint32_t nSta = 4;
  std::string trafficType = "UDP";
  uint32_t payloadSize = 1472;
  uint32_t mpduBufferSize = 64;
  uint64_t simulationTime = 10; // seconds
  std::string phyAntenna = "1";
  std::string phyRxGain = "0";
  std::string phyTxGain = "0";
  std::string phyTxPower = "20";
  std::vector<uint8_t> mcsList = {7, 9, 11, 13}; // Example MCSs (can be parameterized)
  std::vector<uint16_t> widths = {20, 40, 80, 160};
  std::vector<Time> giVals = {NanoSeconds(800), NanoSeconds(1600), NanoSeconds(3200)};
  std::vector<std::string> freqBands = {"2.4", "5", "6"};

  // Parse command line
  CommandLine cmd;
  cmd.AddValue ("mcs", "MCS index", mcs);
  cmd.AddValue ("freqBand", "Operating frequency band: 2.4, 5, or 6", freqBand);
  cmd.AddValue ("uplinkOfdma", "Enable uplink OFDMA", uplinkOfdma);
  cmd.AddValue ("bspr", "Enable BSRP", bspr);
  cmd.AddValue ("nSta", "Number of stations", nSta);
  cmd.AddValue ("trafficType", "TCP or UDP", trafficType);
  cmd.AddValue ("payloadSize", "Application packet size in bytes", payloadSize);
  cmd.AddValue ("mpduBufferSize", "MPDU buffer size", mpduBufferSize);
  cmd.AddValue ("phyAntenna", "Number of antennas", phyAntenna);
  cmd.AddValue ("phyRxGain", "PHY RX gain", phyRxGain);
  cmd.AddValue ("phyTxGain", "PHY TX gain", phyTxGain);
  cmd.AddValue ("phyTxPower", "PHY TX power", phyTxPower);
  cmd.Parse (argc, argv);

  mcsList = {mcs}; // Use specified MCS only unless otherwise desired

  // Frequencies for bands (primary channel numbers)
  std::map<std::string,uint16_t> freqMap = {{"2.4", 1}, {"5", 36}, {"6", 5}};

  // Throughput validation: min/max per MCS/channel/gi (approximated, for demonstration)
  std::map<uint8_t,std::map<uint16_t,std::map<std::string,std::pair<double,double>>>> throughputBoundsMbps;
  // Here just set some placeholder [loose] values; users can refine as needed
  for (auto m: {7,9,11,13})
    for (auto w: {20,40,80,160})
      {
        throughputBoundsMbps[m][w]["0.8us"] = std::make_pair(w*50*m/13, w*80*m/13+10); // crude
        throughputBoundsMbps[m][w]["1.6us"] = std::make_pair(w*40*m/13, w*70*m/13+10);
        throughputBoundsMbps[m][w]["3.2us"] = std::make_pair(w*20*m/13, w*60*m/13+10);
      }

  std::vector<SimResult> results;
  std::ostringstream errorOss;
  bool errorFound = false;

  for (auto freqB : freqBands)
    {
      uint16_t channel = freqMap[freqB];
      for (auto mcsVal : mcsList)
        for (auto widthVal : widths)
          for (auto giVal : giVals)
          {
            std::string giStr = GiToString (giVal);

            // Set up nodes
            NodeContainer wifiStaNodes, wifiApNode;
            wifiStaNodes.Create (nSta);
            wifiApNode.Create (1);

            // Setup wifi channel and PHY
            YansWifiChannelHelper channelHelper = YansWifiChannelHelper::Default ();
            YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
            phy.SetChannel (channelHelper.Create ());
            phy.Set ("Antennas", UintegerValue (std::stoul(phyAntenna)));
            phy.Set ("RxGain", DoubleValue (std::stod(phyRxGain)));
            phy.Set ("TxGain", DoubleValue (std::stod(phyTxGain)));
            phy.Set ("TxPowerStart", DoubleValue (std::stod(phyTxPower)));
            phy.Set ("TxPowerEnd", DoubleValue (std::stod(phyTxPower)));
            phy.Set ("GuardInterval", TimeValue (giVal));
            phy.Set ("ChannelWidth", UintegerValue (widthVal));
            
            // Use IEEE 802.11be
            WifiHelper wifi;
            wifi.SetStandard (WIFI_STANDARD_80211be);

            WifiMacHelper mac;
            Ssid ssid = Ssid ("ns3-11be");

            // Configure BE PHY/MAC attributes
            HtSupportedMcsSet htMcs;
            wifi.SetRemoteStationManager (
              "ns3::HeWifiManager",
              "MaxSupportedMcs", UintegerValue (mcsVal),
              "RtsCtsThreshold", UintegerValue(2347));
            phy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);

            mac.SetType (
              "ns3::StaWifiMac",
              "Ssid", SsidValue (ssid),
              "QosSupported", BooleanValue (true),
              "BE_MaxAmpduSize", UintegerValue (mpduBufferSize*payloadSize));

            NetDeviceContainer staDevices = wifi.Install (phy, mac, wifiStaNodes);

            mac.SetType (
              "ns3::ApWifiMac",
              "Ssid", SsidValue (ssid),
              "QosSupported", BooleanValue (true),
              "BE_MaxAmpduSize", UintegerValue (mpduBufferSize*payloadSize));

            NetDeviceContainer apDevice = wifi.Install (phy, mac, wifiApNode);

            // Set frequency band
            for (uint32_t i = 0; i < staDevices.GetN (); ++i)
              {
                Ptr<WifiNetDevice> wdev = DynamicCast<WifiNetDevice> (staDevices.Get (i));
                wdev->GetPhy ()->SetChannelNumber (channel);
              }
            Ptr<WifiNetDevice> apWdev = DynamicCast<WifiNetDevice> (apDevice.Get (0));
            apWdev->GetPhy ()->SetChannelNumber (channel);

            // Install Internet stack
            InternetStackHelper stack;
            stack.Install (wifiApNode);
            stack.Install (wifiStaNodes);

            // Assign IP addresses
            Ipv4AddressHelper address;
            std::ostringstream subnet;
            subnet << "10." << channel << ".0.0";
            address.SetBase (subnet.str ().c_str (), "255.255.255.0");
            Ipv4InterfaceContainer staIf = address.Assign (staDevices);
            Ipv4InterfaceContainer apIf = address.Assign (apDevice);

            // Mobility
            MobilityHelper mobility;
            Ptr<ListPositionAllocator> apPos = CreateObject<ListPositionAllocator> ();
            apPos->Add (Vector (0.0, 0.0, 0.0));
            mobility.SetPositionAllocator (apPos);
            mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
            mobility.Install (wifiApNode);

            MobilityHelper staMobility;
            Ptr<ListPositionAllocator> staPos = CreateObject<ListPositionAllocator> ();
            for (uint32_t i = 0; i < nSta; ++i)
              {
                staPos->Add (Vector (5.0+(i*2), 0.0, 0.0));
              }
            staMobility.SetPositionAllocator (staPos);
            staMobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
            staMobility.Install (wifiStaNodes);

            uint16_t port = 5000;
            ApplicationContainer serverApps, clientApps;

            if (trafficType == "UDP")
              {
                // UDP: stations send to AP (uplink test)
                UdpServerHelper udpServer (port);
                serverApps = udpServer.Install (wifiApNode.Get (0));
                serverApps.Start (Seconds (0.0));
                serverApps.Stop (Seconds (simulationTime+1));

                for (uint32_t i = 0; i < nSta; ++i)
                  {
                    UdpClientHelper udpClient (apIf.GetAddress (0), port);
                    udpClient.SetAttribute ("MaxPackets", UintegerValue (0));
                    udpClient.SetAttribute ("Interval", TimeValue (Seconds (0.0001)));
                    udpClient.SetAttribute ("PacketSize", UintegerValue (payloadSize));
                    clientApps.Add (udpClient.Install (wifiStaNodes.Get (i)));
                  }
                clientApps.Start (Seconds (1.0));
                clientApps.Stop (Seconds (simulationTime));
              }
            else // TCP
              {
                // TCP: stations send to AP (uplink test)
                for (uint32_t i = 0; i < nSta; ++i)
                  {
                    Address apLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), port+i));
                    PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", apLocalAddress);
                    serverApps.Add (packetSinkHelper.Install (wifiApNode.Get (0)));

                    OnOffHelper clientHelper ("ns3::TcpSocketFactory", InetSocketAddress (apIf.GetAddress (0), port+i));
                    clientHelper.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
                    clientHelper.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
                    clientHelper.SetAttribute ("MaxBytes", UintegerValue (0));
                    clientHelper.SetAttribute ("PacketSize", UintegerValue (payloadSize));
                    clientHelper.SetAttribute ("DataRate", DataRateValue (DataRate ("1Gbps")));
                    clientApps.Add (clientHelper.Install (wifiStaNodes.Get (i)));
                  }
                serverApps.Start (Seconds (0.0));
                serverApps.Stop (Seconds (simulationTime+1));
                clientApps.Start (Seconds (1.0));
                clientApps.Stop (Seconds (simulationTime));
              }
            
            // Throughput measurement
            std::vector<uint64_t> rxBytes (nSta, 0);
            Simulator::Stop (Seconds (simulationTime+2));

            // FlowMonitor
            FlowMonitorHelper flowmon;
            Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

            Simulator::Run ();

            double totalRxBytes = 0;
            if (trafficType == "UDP")
              {
                Ptr<UdpServer> udpServerApp = DynamicCast<UdpServer> (serverApps.Get (0));
                totalRxBytes = udpServerApp->GetReceived () * payloadSize * 8; // bits
              }
            else
              {
                Ptr<PacketSink> sink = DynamicCast<PacketSink> (serverApps.Get (0));
                totalRxBytes = sink->GetTotalRx () * 8; // bits
              }

            monitor->SerializeToXmlFile ("flowmon-results.xml", true, true);

            double throughputMbps = (totalRxBytes/1e6) / simulationTime; // Mbps

            // Validate throughput bounds (approximate)
            double minT = throughputBoundsMbps[mcsVal][widthVal][giStr].first;
            double maxT = throughputBoundsMbps[mcsVal][widthVal][giStr].second;

            bool valid = true;
            CheckThroughput (throughputMbps, minT, maxT, errorOss, mcsVal, widthVal, giStr, errorFound);

            SimResult r;
            r.mcs = mcsVal;
            r.channelWidth = widthVal;
            r.gi = giStr;
            r.throughputMbps = throughputMbps;
            r.valid = valid && !errorFound;
            results.push_back (r);

            Simulator::Destroy ();
            if (errorFound)
              {
                std::cout << errorOss.str();
                return 1;
              }
          }
    }

  std::cout << std::setw(6) << "MCS" << std::setw(12) << "Width(MHz)" << std::setw(10) << "GI" 
            << std::setw(15) << "Throughput(Mbps)" << std::endl;
  for (const auto& r : results)
    {
      std::cout << std::setw(6) << unsigned(r.mcs)
                << std::setw(12) << r.channelWidth
                << std::setw(10) << r.gi
                << std::setw(15) << std::fixed << std::setprecision (2) << r.throughputMbps << std::endl;
    }

  return 0;
}