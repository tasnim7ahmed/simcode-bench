#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/spectrum-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/flow-monitor-module.h"
#include <fstream>
#include <vector>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiPhyComparison");

class ReceptionStats : public Object
{
public:
  ReceptionStats() : m_signalDbm(0), m_noiseDbm(0), m_snrDb(0), m_rxCount(0) {}

  void NotifyRx (Ptr<const Packet> packet, double snr, WifiMode mode, enum WifiPreamble preamble)
  {
    // Intentionally Unused
  }
  void NotifyMonitorSniffRx (Ptr<const Packet> packet, uint16_t channelFreqMhz,
                             WifiTxVector txVector, MpduInfo aMpdu, SignalNoiseDbm sn)
  {
    m_signalDbm += sn.signal;
    m_noiseDbm += sn.noise;
    m_snrDb     += (sn.signal - sn.noise);
    m_rxCount++;
  }
  void Reset ()
  {
    m_signalDbm = m_noiseDbm = m_snrDb = 0;
    m_rxCount = 0;
  }
  double GetAvgSignal() const { return m_rxCount ? m_signalDbm/m_rxCount : 0; }
  double GetAvgNoise()  const { return m_rxCount ? m_noiseDbm/m_rxCount : 0; }
  double GetAvgSnr()    const { return m_rxCount ? m_snrDb/m_rxCount   : 0; }
private:
  double m_signalDbm, m_noiseDbm, m_snrDb;
  uint32_t m_rxCount;
};

int main(int argc, char *argv[])
{
  bool useSpectrum = false;
  bool useUdp = true;
  bool enablePcap = false;
  double simTime = 5.0;
  double distance = 10.0;
  std::string mcsList = "0,1,2,3,4,5,6,7";
  std::string channelWidthList = "20,40";
  std::string guardIntervalList = "800,400";

  CommandLine cmd;
  cmd.AddValue("useSpectrum", "Use SpectrumWifiPhy when true, otherwise YansWifiPhy", useSpectrum);
  cmd.AddValue("useUdp", "Use UDP instead of TCP", useUdp);
  cmd.AddValue("enablePcap", "Enable Pcap packet capture", enablePcap);
  cmd.AddValue("simTime", "Simulation time (s)", simTime);
  cmd.AddValue("distance", "Distance between nodes (m)", distance);
  cmd.AddValue("mcsList", "Comma-separated list of MCS indices", mcsList);
  cmd.AddValue("channelWidthList", "Comma-separated list of channel widths (MHz)", channelWidthList);
  cmd.AddValue("guardIntervalList", "Comma-separated list of guard intervals (ns)", guardIntervalList);
  cmd.Parse(argc,argv);

  std::vector<uint8_t> mcsVector;
  std::vector<uint16_t> channelWidths;
  std::vector<uint16_t> guardIntervals;

  // Parse CSVs
  {
    std::istringstream iss(mcsList);
    std::string token;
    while (std::getline(iss, token, ',')) { mcsVector.push_back(static_cast<uint8_t>(std::stoi(token))); }
    std::istringstream iss2(channelWidthList);
    while (std::getline(iss2, token, ',')) { channelWidths.push_back(static_cast<uint16_t>(std::stoi(token))); }
    std::istringstream iss3(guardIntervalList);
    while (std::getline(iss3, token, ',')) { guardIntervals.push_back(static_cast<uint16_t>(std::stoi(token))); }
  }

  // Print Run Setup
  std::cout << "# PHY: " << (useSpectrum ? "Spectrum" : "Yans")
            << ", Traffic: " << (useUdp ? "UDP" : "TCP")
            << ", Simulation time: " << simTime << " s\n";
  std::cout << "# MCS,ChannelWidth,GI(ns),ThroughputMbps,Signal[dBm],Noise[dBm],SNR[dB]\n";

  for (auto mcsIdx : mcsVector)
  {
    for (auto width : channelWidths)
    {
      for (auto gi : guardIntervals)
      {
        Config::Reset ();

        // Set up Wi-Fi
        NodeContainer wifiStaNode;
        wifiStaNode.Create (1);
        NodeContainer wifiApNode;
        wifiApNode.Create (1);

        // Mobility
        MobilityHelper mobility;
        Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
        positionAlloc->Add (Vector(0.0,0.0,0.0));
        positionAlloc->Add (Vector(distance, 0.0, 0.0));
        mobility.SetPositionAllocator(positionAlloc);
        mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
        mobility.Install(wifiApNode);
        mobility.Install(wifiStaNode);

        WifiHelper wifiHelper;
        wifiHelper.SetStandard (WIFI_STANDARD_80211n_5GHZ);

        WifiMacHelper wifiMac;
        wifiMac.SetType ("ns3::StaWifiMac",
                         "Ssid", SsidValue (Ssid ("ns3-80211n")),
                         "ActiveProbing", BooleanValue (false));

        Ptr<WifiChannel> channel;
        Ptr<WifiPhy> phy;
        Ptr<SpectrumWifiPhy> spectrumPhy;

        if (useSpectrum)
        {
          SpectrumWifiPhyHelper spectrumPhyHelper = SpectrumWifiPhyHelper::Default ();
          Ptr<SpectrumChannel> spectrumChannel = CreateObject<MultiModelSpectrumChannel>();
          spectrumPhyHelper.SetChannel(spectrumChannel);
          spectrumPhyHelper.Set ("ShortGuardEnabled", BooleanValue (gi == 400));
          spectrumPhyHelper.Set ("ChannelWidth", UintegerValue (width));
          spectrumPhy = DynamicCast<SpectrumWifiPhy> (spectrumPhyHelper.Create(wifiStaNode.Get(0), wifiApNode.Get(0)));
          WifiHelper::EnableLogComponents();
          phy = spectrumPhy;
          wifiHelper.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                              "DataMode", StringValue ("HtMcs" + std::to_string(mcsIdx)),
                                              "ControlMode", StringValue ("HtMcs0"));
          NetDeviceContainer staDevice, apDevice;
          staDevice = wifiHelper.Install (spectrumPhyHelper, wifiMac, wifiStaNode);
          wifiMac.SetType ("ns3::ApWifiMac",
                           "Ssid", SsidValue (Ssid ("ns3-80211n")));
          apDevice = wifiHelper.Install (spectrumPhyHelper, wifiMac, wifiApNode);
        }
        else
        {
          YansWifiChannelHelper yansChannel = YansWifiChannelHelper::Default ();
          YansWifiPhyHelper phyHelper = YansWifiPhyHelper::Default ();
          phyHelper.SetChannel (yansChannel.Create ());
          phyHelper.Set ("ShortGuardEnabled", BooleanValue (gi == 400));
          phyHelper.Set ("ChannelWidth", UintegerValue (width));
          phy = DynamicCast<WifiPhy> (phyHelper.Create(wifiStaNode.Get(0), wifiApNode.Get(0)));
          wifiHelper.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                              "DataMode", StringValue ("HtMcs" + std::to_string(mcsIdx)),
                                              "ControlMode", StringValue ("HtMcs0"));
          NetDeviceContainer staDevice, apDevice;
          staDevice = wifiHelper.Install (phyHelper, wifiMac, wifiStaNode);
          wifiMac.SetType ("ns3::ApWifiMac",
                           "Ssid", SsidValue (Ssid ("ns3-80211n")));
          apDevice = wifiHelper.Install (phyHelper, wifiMac, wifiApNode);
        }

        // Internet stack
        InternetStackHelper stack;
        stack.Install (wifiApNode);
        stack.Install (wifiStaNode);

        Ipv4AddressHelper address;
        address.SetBase ("10.0.0.0", "255.255.255.0");
        Ipv4InterfaceContainer staInterface, apInterface;
        NetDeviceContainer staDevice, apDevice;
        // Grab last devices (guaranteed order for 1 AP, 1 STA)
        staDevice.Add (wifiStaNode.Get(0)->GetDevice(0));
        apDevice.Add (wifiApNode.Get(0)->GetDevice(0));
        staInterface = address.Assign(staDevice);
        apInterface = address.Assign(apDevice);

        // Attach reception monitor - only on STA
        Ptr<ReceptionStats> stats = CreateObject<ReceptionStats> ();
        Ptr<WifiNetDevice> staWifiNetDevice = DynamicCast<WifiNetDevice>(staDevice.Get(0));
        if (staWifiNetDevice)
        {
          staWifiNetDevice->GetPhy()->TraceConnectWithoutContext ("MonitorSnifferRx",
            MakeCallback (&ReceptionStats::NotifyMonitorSniffRx, stats));
        }

        uint16_t port = 5000;
        ApplicationContainer sinkApp, sourceApp;

        if (useUdp)
        {
          // UDP traffic: OnOffApplication => PacketSink
          PacketSinkHelper sink ("ns3::UdpSocketFactory", InetSocketAddress (staInterface.GetAddress(0), port));
          sinkApp = sink.Install (wifiStaNode.Get(0));
          sinkApp.Start (Seconds (0.0));
          sinkApp.Stop (Seconds(simTime + 1.0));
          OnOffHelper onoff ("ns3::UdpSocketFactory", InetSocketAddress (staInterface.GetAddress(0), port));
          onoff.SetAttribute ("DataRate", DataRateValue (DataRate ("200Mbps")));
          onoff.SetAttribute ("PacketSize", UintegerValue (1472));
          onoff.SetAttribute ("StartTime", TimeValue (Seconds (1.0)));
          onoff.SetAttribute ("StopTime", TimeValue (Seconds (simTime)));
          onoff.SetAttribute ("MaxBytes", UintegerValue (0)); // unlimited

          sourceApp = onoff.Install (wifiApNode.Get(0));
        }
        else
        {
          // TCP traffic: BulkSendApplication => PacketSink
          PacketSinkHelper sink ("ns3::TcpSocketFactory", InetSocketAddress (staInterface.GetAddress(0), port));
          sinkApp = sink.Install (wifiStaNode.Get(0));
          sinkApp.Start (Seconds (0.0));
          sinkApp.Stop (Seconds(simTime + 1.0));
          BulkSendHelper bulkSend ("ns3::TcpSocketFactory", InetSocketAddress (staInterface.GetAddress(0), port));
          bulkSend.SetAttribute ("MaxBytes", UintegerValue (0)); // unlimited
          sourceApp = bulkSend.Install (wifiApNode.Get(0));
          sourceApp.Start (Seconds (1.0));
          sourceApp.Stop(Seconds (simTime));
        }

        if (enablePcap)
        {
          if (useSpectrum)
          {
            SpectrumWifiPhyHelper::Default ().EnablePcapAll ("phy-" +
              std::to_string(mcsIdx) + "-cw" + std::to_string(width) + "-gi" + std::to_string(gi));
          }
          else
          {
            YansWifiPhyHelper::Default ().EnablePcapAll ("phy-" +
              std::to_string(mcsIdx) + "-cw" + std::to_string(width) + "-gi" + std::to_string(gi));
          }
        }

        // Run simulation
        Simulator::Stop (Seconds (simTime + 1.0));
        Simulator::Run ();

        // Throughput measurement
        double rxBytes = DynamicCast<PacketSink> (sinkApp.Get(0))->GetTotalRx ();
        double throughput = rxBytes * 8.0 / (simTime - 1.0) / 1e6; // Mbps

        std::cout << int(mcsIdx) << "," << width << "," << gi << ",";
        std::cout << throughput << ",";
        std::cout << stats->GetAvgSignal() << "," << stats->GetAvgNoise() << "," << stats->GetAvgSnr() << "\n";

        Simulator::Destroy();
      }
    }
  }

  std::cout << "# To toggle UDP/TCP, set useUdp={1/0}.\n";
  std::cout << "# To capture packets, set enablePcap=1.\n";
  std::cout << "# Run with:\n";
  std::cout << "#   ./waf --run \"thisfile --useSpectrum=1 --enablePcap=1 --simTime=10 --distance=15 --useUdp=1\"\n";
  return 0;
}