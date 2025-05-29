#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/spectrum-module.h"
#include "ns3/propagation-module.h"
#include "ns3/gnuplot.h"

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiErrorRateModels");

int main (int argc, char *argv[])
{
  bool enableGnuplot = true;
  uint32_t frameSize = 1000;
  double simulationTime = 10; //seconds
  double startSNR = -5;
  double stopSNR = 30;
  uint32_t numPoints = 36; // 1 dB increments

  CommandLine cmd (__FILE__);
  cmd.AddValue ("enableGnuplot", "Enable Gnuplot output", enableGnuplot);
  cmd.AddValue ("frameSize", "Frame size in bytes", frameSize);
  cmd.Parse (argc, argv);

  LogComponent::SetFilter ("WifiErrorRateModels", LOG_LEVEL_INFO);

  std::vector<std::string> errorRateModels = {"NistErrorRateModel", "YansErrorRateModel", "TableBasedErrorRateModel"};
  std::vector<std::string> ofdmRates = {"OfdmRate6Mbps", "OfdmRate9Mbps", "OfdmRate12Mbps", "OfdmRate18Mbps", "OfdmRate24Mbps", "OfdmRate36Mbps", "OfdmRate48Mbps", "OfdmRate54Mbps"};

  for (const auto& modelName : errorRateModels)
  {
    for (const auto& rate : ofdmRates)
    {
      std::cout << "Running simulation for error rate model: " << modelName << " and rate: " << rate << std::endl;

      std::vector<double> snrValues;
      std::vector<double> frameSuccessRates;

      for (uint32_t i = 0; i <= numPoints; ++i)
      {
        double snr = startSNR + (stopSNR - startSNR) * (double)i / (double)numPoints;

        NodeContainer nodes;
        nodes.Create (2);

        WifiHelper wifi;
        wifi.SetStandard (WIFI_PHY_STANDARD_80211a);

        YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
        wifiPhy.SetPcapDataLinkType (YansWifiPhyHelper::DLT_IEEE802_11_RADIO);
        wifiPhy.Set ("TxPowerStart", DoubleValue (16));
        wifiPhy.Set ("TxPowerEnd", DoubleValue (16));
        wifiPhy.Set ("RxGain", DoubleValue (0));

        Config::SetDefault ("ns3::RangePropagationLossModel::MaxRange", DoubleValue (1000));

        YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
        wifiChannel.AddPropagationLoss ("ns3::LogDistancePropagationLossModel", "Exponent", DoubleValue (3));
        wifiChannel.AddPropagationLoss("ns3::ConstantLossPropagationModel", "ReferenceLoss", DoubleValue(-snr));

        if (modelName == "NistErrorRateModel") {
            wifiPhy.SetErrorRateModel ("ns3::NistErrorRateModel", "SpectrumModel", StringValue ("ns3::SpectrumWifiModel"));
        } else if (modelName == "YansErrorRateModel") {
            wifiPhy.SetErrorRateModel ("ns3::YansErrorRateModel");
        } else {
            wifiPhy.SetErrorRateModel ("ns3::TableBasedErrorRateModel");
        }

        wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
        Ptr<YansWifiChannel> chan = wifiChannel.Create ();
        wifiPhy.SetChannel (chan);

        WifiMacHelper wifiMac;
        wifiMac.SetType ("ns3::AdhocWifiMac");
        Ssid ssid = Ssid ("ns-3-ssid");
        wifiMac.SetSSID (ssid);

        NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, nodes);

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

        Ipv4AddressHelper ipv4;
        ipv4.SetBase ("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer i = ipv4.Assign (devices);

        uint16_t port = 9;
        UdpEchoServerHelper echoServer (port);

        ApplicationContainer serverApps = echoServer.Install (nodes.Get (1));
        serverApps.Start (Seconds (1.0));
        serverApps.Stop (Seconds (simulationTime - 1));

        UdpEchoClientHelper echoClient (i.GetAddress (1), port);
        echoClient.SetAttribute ("MaxPackets", UintegerValue (100));
        echoClient.SetAttribute ("Interval", TimeValue (Seconds (0.1)));
        echoClient.SetAttribute ("PacketSize", UintegerValue (frameSize));

        ApplicationContainer clientApps = echoClient.Install (nodes.Get (0));
        clientApps.Start (Seconds (2.0));
        clientApps.Stop (Seconds (simulationTime - 2));

        Simulator::Stop (Seconds (simulationTime));

        uint32_t totalPacketsSent = 0;
        uint32_t totalPacketsReceived = 0;

        Simulator::Run ();

        Ptr<UdpEchoClient> client = DynamicCast<UdpEchoClient> (clientApps.Get (0));
        totalPacketsSent = client->GetPacketsSent ();

        Ptr<UdpEchoServer> server = DynamicCast<UdpEchoServer> (serverApps.Get (0));
        totalPacketsReceived = server->GetPacketsReceived ();

        double frameSuccessRate = (double)totalPacketsReceived / (double)totalPacketsSent;

        std::cout << "SNR: " << snr << " dB, Frame Success Rate: " << frameSuccessRate << std::endl;

        snrValues.push_back (snr);
        frameSuccessRates.push_back (frameSuccessRate);

        Simulator::Destroy ();
      }

      if (enableGnuplot)
      {
        std::string filename = modelName + "_" + rate + ".plt";
        Gnuplot plot (filename);
        plot.SetTitle ("Frame Success Rate vs. SNR (" + modelName + ", " + rate + ")");
        plot.SetLegend ("SNR (dB)", "Frame Success Rate");
        plot.AddDataset (snrValues, frameSuccessRates);

        std::ofstream outfile (modelName + "_" + rate + ".dat");
        for (size_t i = 0; i < snrValues.size (); ++i)
        {
          outfile << snrValues[i] << " " << frameSuccessRates[i] << std::endl;
        }
        outfile.close ();

        plot.GenerateOutput (std::cout);
      }
    }
  }

  return 0;
}