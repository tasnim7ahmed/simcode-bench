#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/spectrum-module.h"
#include "ns3/propagation-loss-model.h"
#include "ns3/propagation-delay-model.h"
#include "ns3/gnuplot.h"
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <cmath>

using namespace ns3;

int main(int argc, char *argv[]) {
  uint32_t packetSize = 1472;
  double simulationTime = 10;

  CommandLine cmd;
  cmd.AddValue("packetSize", "Size of packet generated", packetSize);
  cmd.Parse(argc, argv);

  Config::SetDefault("ns3::WifiMacQueue::MaxSize", StringValue("65535"));

  std::vector<std::string> errorModels = {"NistErrorRateModel",
                                          "YansErrorRateModel",
                                          "TableBasedErrorRateModel"};
  std::vector<std::string> modes = {"VhtMcs0",  "VhtMcs1",  "VhtMcs2",
                                    "VhtMcs3",  "VhtMcs4",  "VhtMcs5",
                                    "VhtMcs6",  "VhtMcs7",  "VhtMcs8",
                                    "VhtMcs9"}; // Include MCS9 for all but 20 MHz
  std::vector<int> channelWidths = {20, 40, 80, 160};
  double startSnr = -5;
  double endSnr = 30;
  double snrStep = 1;

  for (auto channelWidth : channelWidths) {
    for (auto errorModel : errorModels) {
      for (auto modeStr : modes) {
        if (channelWidth == 20 && modeStr == "VhtMcs9") continue;

        std::string modeName = "Ht80MHzVht" + modeStr;
        if(channelWidth==20)
            modeName = "Ht20MHzVht" + modeStr;
        else if (channelWidth==40)
            modeName = "Ht40MHzVht" + modeStr;
        else if (channelWidth==160)
            modeName = "Ht160MHzVht" + modeStr;
        WifiMode mode = WifiMode(modeName);
        std::string dataFile =
            "frame-success-rate-" + std::to_string(channelWidth) + "MHz-" +
            errorModel + "-" + modeStr + ".dat";
        std::ofstream os;
        os.open(dataFile.c_str());

        for (double snr = startSnr; snr <= endSnr; snr += snrStep) {
          NodeContainer nodes;
          nodes.Create(2);

          WifiHelper wifi;
          wifi.SetStandard(WIFI_PHY_STANDARD_80211ac);

          YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
          YansWifiPhyHelper phy;
          phy.SetChannel(channel.Create());
          phy.Set("ChannelWidth", UintegerValue(channelWidth));

          Config::Set("/Channel/PropagationLossModel/ReferenceLoss",
                      DoubleValue(46.6777));
          Config::Set("/Channel/PropagationLossModel/Frequency",
                      DoubleValue(5.18e9));

          phy.Set("TxGain", DoubleValue(10));
          phy.Set("RxGain", DoubleValue(10));
          phy.Set("RxNoiseFigure", DoubleValue(7));
          phy.Set("ThermalNoise", DoubleValue(-93.97));

          Config::Set("/Channel/PropagationLossModel/Exponent",
                      DoubleValue(2.0));
          Config::Set("/Channel/PropagationLossModel/DistanceReference",
                      DoubleValue(1));

          if (errorModel == "NistErrorRateModel") {
            phy.SetErrorRateModel("ns3::NistErrorRateModel");
          } else if (errorModel == "YansErrorRateModel") {
            phy.SetErrorRateModel("ns3::YansErrorRateModel");
          } else if (errorModel == "TableBasedErrorRateModel") {
            phy.SetErrorRateModel("ns3::TableBasedErrorRateModel");
          } else {
            std::cerr << "Unknown error model" << std::endl;
            return 1;
          }
          Config::Set("/NodeList/*/$ns3::WifiNetDevice/Phy/ChannelNumber",
                      UintegerValue(36));

          WifiMacHelper mac;
          NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

          MobilityHelper mobility;
          mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                        "MinX", DoubleValue(0.0), "MinY",
                                        DoubleValue(0.0), "DeltaX",
                                        DoubleValue(5.0), "DeltaY",
                                        DoubleValue(10.0), "GridWidth",
                                        UintegerValue(3), "LayoutType",
                                        StringValue("RowFirst"));

          mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
          mobility.Install(nodes);

          InternetStackHelper internet;
          internet.Install(nodes);

          Ipv4AddressHelper ipv4;
          ipv4.SetBase("10.1.1.0", "255.255.255.0");
          Ipv4InterfaceContainer i = ipv4.Assign(devices);

          uint16_t port = 9;

          PacketSinkHelper sink("ns3::UdpSocketFactory",
                                InetSocketAddress(i.GetAddress(1), port));
          ApplicationContainer sinkApps = sink.Install(nodes.Get(1));
          sinkApps.Start(Seconds(0.0));
          sinkApps.Stop(Seconds(simulationTime + 1));

          Ptr<Socket> ns3UdpSocket =
              Socket::CreateSocket(nodes.Get(0), UdpSocketFactory::GetTypeId());
          ns3UdpSocket->Bind();
          Ptr<Application> app = CreateObject<OnOffApplication>(ns3UdpSocket);
          app->SetStartTime(Seconds(1.0));
          app->SetStopTime(Seconds(simulationTime));
          Config::Set("/NodeList/*/ApplicationList/*/$ns3::OnOffApplication/PacketSize", UintegerValue(packetSize));
          Config::Set("/NodeList/*/ApplicationList/*/$ns3::OnOffApplication/OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
          Config::Set("/NodeList/*/ApplicationList/*/$ns3::OnOffApplication/OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
          Ptr<OnOffApplication> onOffApp = DynamicCast<OnOffApplication>(app);
          onOffApp->SetRemote(InetSocketAddress(i.GetAddress(1), port));
          ApplicationContainer appContainer;
          appContainer.Add(app);
          appContainer.Install(nodes.Get(0));
          appContainer.Start(Seconds(1.0));
          appContainer.Stop(Seconds(simulationTime));

          Simulator::Stop(Seconds(simulationTime + 1));

          phy.Set("Snr", DoubleValue(snr));

          Simulator::Run();

          uint64_t totalBytes = DynamicCast<PacketSink>(sinkApps.Get(0))->GetTotalRx();
          double frameSuccessRate =
              (double)totalBytes / (simulationTime - 1) / packetSize / 8;

          os << snr << " " << frameSuccessRate << std::endl;
          Simulator::Destroy();
        }
        os.close();

        std::string plotFile =
            "frame-success-rate-" + std::to_string(channelWidth) + "MHz-" +
            errorModel + "-" + modeStr + ".plt";
        std::ofstream plotOs;
        plotOs.open(plotFile.c_str());
        plotOs << "set terminal png size 1024,768" << std::endl;
        plotOs << "set output \"" << "frame-success-rate-" +
                      std::to_string(channelWidth) + "MHz-" + errorModel + "-" +
                      modeStr + ".png\""
               << std::endl;
        plotOs << "set title \"Frame Success Rate vs. SNR ("
               << std::to_string(channelWidth) << "MHz," << errorModel << ","
               << modeStr << ")\"" << std::endl;
        plotOs << "set xlabel \"SNR (dB)\"" << std::endl;
        plotOs << "set ylabel \"Frame Success Rate\"" << std::endl;
        plotOs << "plot \"" << dataFile << "\" using 1:2 with linespoints"
               << std::endl;
        plotOs.close();

        std::string command = "gnuplot " + plotFile;
        system(command.c_str());
      }
    }
  }

  return 0;
}