#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/gnuplot.h"
#include "ns3/command-line.h"
#include "ns3/config-store.h"
#include <iostream>
#include <fstream>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("VhtErrorRateModels");

int main (int argc, char *argv[])
{
  bool verbose = false;
  uint32_t frameSize = 1000;

  CommandLine cmd;
  cmd.AddValue ("verbose", "Tell application to log if enabled.", verbose);
  cmd.AddValue ("frameSize", "Frame size in bytes", frameSize);
  cmd.Parse (argc,argv);

  if (verbose)
    {
      LogComponentEnable ("VhtErrorRateModels", LOG_LEVEL_ALL);
    }

  Config::SetDefault ("ns3::WifiMacQueue::MaxSize", StringValue ("65535"));
  Config::SetDefault ("ns3::WifiMacQueue::Mode", EnumValue (WifiMacQueue::ADAPTIVE));

  std::vector<std::string> errorRateModels = {"NistErrorRateModel", "YansErrorRateModel", "TableBasedErrorRateModel"};
  std::vector<uint32_t> mcsValues;
  for (uint32_t i = 0; i <= 9; ++i) {
    mcsValues.push_back(i);
  }
  mcsValues.erase(std::remove(mcsValues.begin(), mcsValues.end(), 9), mcsValues.end());

  double minSnr = -5.0;
  double maxSnr = 30.0;
  double snrStep = 1.0;

  for (const std::string& errorRateModel : errorRateModels) {
    for (uint32_t mcs : mcsValues) {
      std::vector<double> snrValues;
      std::vector<double> successRates;

      for (double snr = minSnr; snr <= maxSnr; snr += snrStep) {
        NodeContainer nodes;
        nodes.Create (2);

        WifiHelper wifi;
        wifi.SetStandard (WIFI_PHY_STANDARD_80211ac);

        YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
        YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
        phy.SetChannel (channel.Create ());
        phy.SetErrorRateModel (errorRateModel);
        phy.Set ("ChannelWidth", UintegerValue (20));
        phy.Set ("ShortGuardEnabled", BooleanValue (true));
        phy.Set ("Antennas", UintegerValue (1));

        WifiMacHelper mac;
        Ssid ssid = Ssid ("ns-3-ssid");
        mac.SetType ("ns3::StaWifiMac",
                     "Ssid", SsidValue (ssid),
                     "ActiveProbing", BooleanValue (false));

        NetDeviceContainer staDevices;
        staDevices = wifi.Install (phy, mac, nodes.Get (0));

        mac.SetType ("ns3::ApWifiMac",
                     "Ssid", SsidValue (ssid),
                     "BeaconGeneration", BooleanValue (true),
                     "BeaconInterval", TimeValue (Seconds (1.0)),
                     "QosSupported", BooleanValue (true),
                     "HtSupported", BooleanValue (true),
                     "VhtSupported", BooleanValue (true));

        NetDeviceContainer apDevices;
        apDevices = wifi.Install (phy, mac, nodes.Get (1));

        MobilityHelper mobility;
        mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                      "MinX", DoubleValue (0.0),
                                      "MinY", DoubleValue (0.0),
                                      "DeltaX", DoubleValue (5.0),
                                      "DeltaY", DoubleValue (10.0),
                                      "GridWidth", UintegerValue (3),
                                      "LayoutType", StringValue ("RowFirst"));
        mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
        mobility.Install (nodes);

        InternetStackHelper internet;
        internet.Install (nodes);

        Ipv4AddressHelper address;
        address.SetBase ("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = address.Assign (staDevices);
        address.Assign (apDevices);

        UdpEchoServerHelper echoServer (9);
        ApplicationContainer serverApps = echoServer.Install (nodes.Get (1));
        serverApps.Start (Seconds (0.0));
        serverApps.Stop (Seconds (1.0));

        UdpEchoClientHelper echoClient (interfaces.GetAddress (1), 9);
        echoClient.SetAttribute ("MaxPackets", UintegerValue (1));
        echoClient.SetAttribute ("Interval", TimeValue (Seconds (0.1)));
        echoClient.SetAttribute ("PacketSize", UintegerValue (frameSize));
        ApplicationContainer clientApps = echoClient.Install (nodes.Get (0));
        clientApps.Start (Seconds (0.1));
        clientApps.Stop (Seconds (1.0));

        Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/TxPowerEnd", DoubleValue (snr));
        Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/TxPowerStart", DoubleValue (snr));

        Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ChannelWidth", UintegerValue (20));
        Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/GuardInterval", EnumValue (WifiPhy::GI_SHORT));

        Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/HtConfiguration/ShortGuardIntervalSupported", BooleanValue(true));
        Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/VhtConfiguration/ShortGuardIntervalSupported", BooleanValue(true));

        std::stringstream rateStr;
        rateStr << "VhtMcs" << mcs;
        std::string rate = rateStr.str();

        Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/VhtConfiguration/TxStream", UintegerValue (1));
        Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/VhtConfiguration/RxStream", UintegerValue (1));

        Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/VhtConfiguration/Supported", BooleanValue (true));
        Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/HtConfiguration/Supported", BooleanValue (true));
        Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ControlMode", StringValue (rate));
        Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/DataMode", StringValue (rate));

        Simulator::Stop (Seconds (1.1));
        Simulator::Run ();

        uint32_t totalTxPackets = 0;
        uint32_t totalRxPackets = 0;
        Ptr<Application> app = clientApps.Get(0);
        Ptr<UdpEchoClient> echo = DynamicCast<UdpEchoClient>(app);
        totalTxPackets = echo->GetTxPackets();
        app = serverApps.Get(0);
        Ptr<UdpEchoServer> echoServerApp = DynamicCast<UdpEchoServer>(app);
        totalRxPackets = echoServerApp->GetRxPackets();

        double successRate = 0.0;
        if (totalTxPackets > 0) {
            successRate = (double)totalRxPackets / totalTxPackets;
        }

        snrValues.push_back(snr);
        successRates.push_back(successRate);

        Simulator::Destroy ();
      }

      std::string plotFilename = errorRateModel + "-VhtMcs" + std::to_string(mcs) + ".plt";
      std::string dataFilename = errorRateModel + "-VhtMcs" + std::to_string(mcs) + ".dat";

      Gnuplot plot (plotFilename);
      plot.SetTitle (errorRateModel + " - VhtMcs" + std::to_string(mcs) + " - Frame Size: " + std::to_string(frameSize) + " Bytes");
      plot.SetLegend ("SNR (dB)", "Frame Success Rate");

      Gnuplot2dDataset dataset;
      dataset.SetTitle ("Success Rate");
      for (size_t i = 0; i < snrValues.size(); ++i) {
        dataset.Add (snrValues[i], successRates[i]);
      }

      plot.AddDataset (dataset);

      std::ofstream dataFile (dataFilename.c_str());
      for(size_t i = 0; i < snrValues.size(); ++i)
      {
        dataFile << snrValues[i] << " " << successRates[i] << std::endl;
      }
      dataFile.close();

      plot.GenerateOutput (std::cout);
    }
  }

  return 0;
}