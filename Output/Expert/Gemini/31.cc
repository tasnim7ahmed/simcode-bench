#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/spectrum-module.h"
#include "ns3/propagation-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/gnuplot.h"
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  bool enablePcap = false;
  uint32_t frameSize = 1200;
  double minSnr = 0.0;
  double maxSnr = 20.0;
  uint32_t numSnrPoints = 21;

  std::vector<std::string> errorRateModels = {"NistErrorRateModel", "YansErrorRateModel", "TableBasedErrorRateModel"};
  std::vector<int> htMcsValues = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};

  for (const std::string& errorRateModel : errorRateModels) {
    for (int htMcs : htMcsValues) {
      std::string filename = "ht_mcs_" + std::to_string(htMcs) + "_" + errorRateModel + ".dat";
      std::ofstream output(filename);
      output << "# SNR FrameSuccessRate" << std::endl;

      for (uint32_t i = 0; i < numSnrPoints; ++i) {
        double snr = minSnr + (maxSnr - minSnr) * i / (numSnrPoints - 1);
        uint32_t numTrials = 1000;
        uint32_t numSuccesses = 0;

        for (uint32_t j = 0; j < numTrials; ++j) {
          NodeContainer nodes;
          nodes.Create(2);

          WifiHelper wifi;
          wifi.SetStandard(WIFI_PHY_STANDARD_80211n);
          YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
          YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
          wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
          wifiChannel.AddPropagationLoss("ns3::FriisPropagationLossModel");

          wifiPhy.SetChannel(wifiChannel.Create());
          wifiPhy.Set("TxPowerStart", DoubleValue(10.0));
          wifiPhy.Set("TxPowerEnd", DoubleValue(10.0));
          wifiPhy.Set("RxSensitivity", DoubleValue(-90.0));
          if (errorRateModel == "NistErrorRateModel") {
              wifiPhy.SetErrorRateModel("ns3::NistErrorRateModel");
          } else if (errorRateModel == "YansErrorRateModel") {
              wifiPhy.SetErrorRateModel("ns3::YansErrorRateModel");
          } else {
              wifiPhy.SetErrorRateModel("ns3::TableBasedErrorRateModel",
                                        "LookupTableFilename", StringValue("src/wifi/model/wifi-mcs-error-model.txt"));
          }
          wifiPhy.Set("Snr", DoubleValue(snr));


          WifiMacHelper wifiMac;
          Ssid ssid = Ssid("wifi-default");
          wifiMac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));

          NetDeviceContainer staDevices;
          staDevices = wifi.Install(wifiPhy, wifiMac, nodes.Get(0));

          wifiMac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));

          NetDeviceContainer apDevices;
          apDevices = wifi.Install(wifiPhy, wifiMac, nodes.Get(1));

          MobilityHelper mobility;
          mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                        "MinX", DoubleValue(0.0),
                                        "MinY", DoubleValue(0.0),
                                        "DeltaX", DoubleValue(5.0),
                                        "DeltaY", DoubleValue(10.0),
                                        "GridWidth", UintegerValue(3),
                                        "LayoutType", StringValue("RowFirst"));
          mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
          mobility.Install(nodes);

          InternetStackHelper internet;
          internet.Install(nodes);

          Ipv4AddressHelper ipv4;
          ipv4.SetBase("10.1.1.0", "255.255.255.0");
          Ipv4InterfaceContainer i = ipv4.Assign(staDevices);
          i = ipv4.Assign(apDevices);

          UdpClientHelper client(i.GetAddress(1), 9);
          client.SetAttribute("MaxPackets", UintegerValue(1));
          client.SetAttribute("Interval", TimeValue(Seconds(0.00001)));
          client.SetAttribute("PacketSize", UintegerValue(frameSize));

          ApplicationContainer clientApps = client.Install(nodes.Get(0));
          clientApps.Start(Seconds(1.0));
          clientApps.Stop(Seconds(1.1));

          PacketSinkHelper sink("ns3::UdpSocketFactory",
                                InetSocketAddress(Ipv4Address::GetAny(), 9));
          ApplicationContainer sinkApps = sink.Install(nodes.Get(1));
          sinkApps.Start(Seconds(1.0));
          sinkApps.Stop(Seconds(1.1));

          Config::ConnectWithoutContext("/NodeList/1/ApplicationList/0/$ns3::PacketSink/Rx", MakeCallback (&( [] (Ptr<const Packet> p, const Address &addr) {
            Simulator::Stop(Seconds(0.0));
          })));

          Simulator::Stop(Seconds(1.2));
          Simulator::Run();

          if(Simulator::Now() < Seconds(1.2))
          {
            numSuccesses++;
          }

          Simulator::Destroy();
        }

        double frameSuccessRate = (double)numSuccesses / numTrials;
        output << snr << " " << frameSuccessRate << std::endl;
        std::cout << "HT MCS: " << htMcs << ", Error Model: " << errorRateModel << ", SNR: " << snr << ", Frame Success Rate: " << frameSuccessRate << std::endl;
      }
      output.close();

      std::string plotFilename = "ht_mcs_" + std::to_string(htMcs) + "_" + errorRateModel + ".plt";
      std::ofstream plotFile(plotFilename);
      plotFile << "set terminal png size 640,480" << std::endl;
      plotFile << "set output \"" << "ht_mcs_" + std::to_string(htMcs) + "_" + errorRateModel + ".png\"" << std::endl;
      plotFile << "set title \"HT MCS " << htMcs << ", Error Model: " << errorRateModel << "\"" << std::endl;
      plotFile << "set xlabel \"SNR (dB)\"" << std::endl;
      plotFile << "set ylabel \"Frame Success Rate\"" << std::endl;
      plotFile << "plot \"" << filename << "\" using 1:2 with lines title \"" << errorRateModel << "\"" << std::endl;
      plotFile.close();

      std::string command = "gnuplot " + plotFilename;
      system(command.c_str());
    }
  }

  return 0;
}