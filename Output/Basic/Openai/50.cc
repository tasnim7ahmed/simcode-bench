#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/spectrum-wifi-helper.h"
#include "ns3/packet-sink.h"
#include "ns3/propagation-loss-model.h"
#include "ns3/error-model.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiMcsChannelWidthGuardIntervalThroughput");

struct SimulationResults
{
  uint32_t mcs;
  uint32_t channelWidth;
  bool shortGi;
  double throughput;
  std::string phyMode;
};

std::string GetModulationScheme (uint8_t mcs)
{
  static std::string mcsMod[] =
    {
      "BPSK 1/2", "QPSK 1/2", "QPSK 3/4", "16-QAM 1/2", "16-QAM 3/4", "64-QAM 2/3", "64-QAM 3/4", "64-QAM 5/6",
      "256-QAM 3/4", "256-QAM 5/6"
    };
  return mcs < 10 ? mcsMod[mcs] : "Unknown";
}

int
main (int argc, char *argv[])
{
  double simulationTime = 10; // seconds
  double distance = 10; // meters
  std::string phyType = "YansWifiPhy"; // or SpectrumWifiPhy
  std::string errorModelType = "Nist";
  uint32_t maxMcs = 9;
  std::vector<uint32_t> channelWidths = {20, 40, 80};
  CommandLine cmd;
  cmd.AddValue ("simulationTime", "Duration of the simulation [s]", simulationTime);
  cmd.AddValue ("distance", "Distance between STA and AP [m]", distance);
  cmd.AddValue ("phyType", "Wi-Fi PHY implementation: YansWifiPhy or SpectrumWifiPhy", phyType);
  cmd.AddValue ("errorModelType", "Error model: Nist or Yans", errorModelType);
  cmd.AddValue ("maxMcs", "Maximum MCS index (0 .. 9)", maxMcs);
  cmd.Parse (argc, argv);

  std::vector<bool> shortGiValues = {false, true};
  std::vector<SimulationResults> results;

  for (uint32_t chIdx = 0; chIdx < channelWidths.size (); ++chIdx)
    {
      for (uint32_t mcs = 0; mcs <= maxMcs; ++mcs)
        {
          for (bool shortGi : shortGiValues)
            {
              // Clean up previous simulation instance
              Simulator::Destroy ();

              NodeContainer wifiStaNode;
              wifiStaNode.Create (1);
              NodeContainer wifiApNode;
              wifiApNode.Create (1);

              YansWifiChannelHelper yansChannel = YansWifiChannelHelper::Default ();
              yansChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
              yansChannel.AddPropagationLoss ("ns3::FriisPropagationLossModel");

              Ptr<YansWifiChannel> channel = yansChannel.Create ();
              Ptr<Channel> commonChannel = channel;

              WifiHelper wifi;
              wifi.SetStandard (WIFI_STANDARD_80211ac);
              wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                            "DataMode", StringValue ("VhtMcs" + std::to_string (mcs)),
                                            "ControlMode", StringValue ("VhtMcs0"));

              NetDeviceContainer staDevice, apDevice;
              if (phyType == "SpectrumWifiPhy")
                {
                  SpectrumWifiPhyHelper phy = SpectrumWifiPhyHelper::Default ();
                  phy.SetChannel (CreateObject<SpectrumChannel> ());
                  phy.Set ("ShortGuardEnabled", BooleanValue (shortGi));
                  phy.Set ("ChannelWidth", UintegerValue (channelWidths[chIdx]));
                  WifiMacHelper mac;
                  Ssid ssid = Ssid ("wifi-mcs-test");
                  mac.SetType ("ns3::StaWifiMac",
                               "Ssid", SsidValue (ssid),
                               "ActiveProbing", BooleanValue (false));
                  staDevice = wifi.Install (phy, mac, wifiStaNode);

                  mac.SetType ("ns3::ApWifiMac", "Ssid", SsidValue (ssid));
                  apDevice = wifi.Install (phy, mac, wifiApNode);
                }
              else // YansWifiPhy (default)
                {
                  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
                  phy.SetChannel (channel);
                  phy.Set ("ShortGuardEnabled", BooleanValue (shortGi));
                  phy.Set ("ChannelWidth", UintegerValue (channelWidths[chIdx]));
                  WifiMacHelper mac;
                  Ssid ssid = Ssid ("wifi-mcs-test");
                  mac.SetType ("ns3::StaWifiMac",
                               "Ssid", SsidValue (ssid),
                               "ActiveProbing", BooleanValue (false));
                  staDevice = wifi.Install (phy, mac, wifiStaNode);

                  mac.SetType ("ns3::ApWifiMac", "Ssid", SsidValue (ssid));
                  apDevice = wifi.Install (phy, mac, wifiApNode);
                }

              // Optionally set error model
              if (errorModelType != "")
                {
                  Ptr<ErrorRateModel> errModel;
                  if (errorModelType == "Nist")
                    errModel = CreateObject<NistErrorRateModel> ();
                  else if (errorModelType == "Yans")
                    errModel = CreateObject<YansErrorRateModel> ();
                  else
                    errModel = CreateObject<NistErrorRateModel> ();
                  for (auto dev : {staDevice.Get (0), apDevice.Get (0)})
                    {
                      Ptr<WifiNetDevice> wifiDev = DynamicCast<WifiNetDevice> (dev);
                      if (wifiDev)
                        {
                          Ptr<WifiPhy> wifiPhy = wifiDev->GetPhy ();
                          wifiPhy->SetErrorRateModel (errModel);
                        }
                    }
                }

              MobilityHelper mobility;
              Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
              positionAlloc->Add (Vector (0.0, 0.0, 0.0));
              positionAlloc->Add (Vector (distance, 0.0, 0.0));
              mobility.SetPositionAllocator (positionAlloc);
              mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
              mobility.Install (wifiApNode);
              mobility.Install (wifiStaNode);

              InternetStackHelper stack;
              stack.Install (wifiApNode);
              stack.Install (wifiStaNode);

              Ipv4AddressHelper address;
              address.SetBase ("10.1.1.0", "255.255.255.0");
              Ipv4InterfaceContainer staIface, apIface;
              apIface = address.Assign (apDevice);
              staIface = address.Assign (staDevice);

              uint16_t port = 59744;
              // UDP Packet Sink (receiver) on AP
              ApplicationContainer sinkApp;
              PacketSinkHelper sinkHelper ("ns3::UdpSocketFactory", InetSocketAddress (apIface.GetAddress (0), port));
              sinkApp = sinkHelper.Install (wifiApNode.Get (0));
              sinkApp.Start (Seconds (0.0));
              sinkApp.Stop (Seconds (simulationTime + 1.0));

              // UDP Traffic generator on STA
              OnOffHelper onoff ("ns3::UdpSocketFactory", InetSocketAddress (apIface.GetAddress (0), port));
              onoff.SetAttribute ("DataRate", DataRateValue (DataRate ("500Mbps")));
              onoff.SetAttribute ("PacketSize", UintegerValue (1472));
              onoff.SetAttribute ("StartTime", TimeValue (Seconds (1.0)));
              onoff.SetAttribute ("StopTime", TimeValue (Seconds (simulationTime)));

              ApplicationContainer clientApp = onoff.Install (wifiStaNode.Get (0));

              Simulator::Stop (Seconds (simulationTime + 1.0));
              Simulator::Run ();

              Ptr<PacketSink> sink = DynamicCast<PacketSink> (sinkApp.Get (0));
              double rxBytes = sink->GetTotalRx ();
              double throughput = (rxBytes * 8.0) / (simulationTime - 1) / 1e6; // Mbps

              SimulationResults entry;
              entry.mcs = mcs;
              entry.channelWidth = channelWidths[chIdx];
              entry.shortGi = shortGi;
              entry.throughput = throughput;
              entry.phyMode = "VhtMcs" + std::to_string (mcs) + " (" + GetModulationScheme (mcs) + ")";
              results.push_back (entry);

              Simulator::Destroy ();
            }
        }
    }

  std::cout << "MCS,ChannelWidth(MHz),ShortGI,Modulation,Throughput(Mbps)" << std::endl;
  for (const auto &result : results)
    {
      std::cout << result.mcs << "," << result.channelWidth << ","
                << (result.shortGi ? "Yes" : "No") << ","
                << result.phyMode << ","
                << result.throughput << std::endl;
    }

  return 0;
}