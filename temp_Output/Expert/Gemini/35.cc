#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cmath>

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/spectrum-module.h"
#include "ns3/propagation-module.h"
#include "ns3/gnuplot.h"
#include "ns3/command-line.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  uint32_t frameSize = 1000;
  double simulationTime = 10.0;

  CommandLine cmd;
  cmd.AddValue("frameSize", "Frame size in bytes", frameSize);
  cmd.Parse(argc, argv);

  std::vector<std::string> ofdmModes = {
    "OfdmRate6Mbps", "OfdmRate9Mbps", "OfdmRate12Mbps",
    "OfdmRate18Mbps", "OfdmRate24Mbps", "OfdmRate36Mbps",
    "OfdmRate48Mbps", "OfdmRate54Mbps"
  };

  std::vector<std::string> errorModels = {"NistErrorRateModel", "YansErrorRateModel", "TableBasedErrorRateModel"};

  for (const auto& mode : ofdmModes) {
    for (const auto& errorModelName : errorModels) {
      std::string dataFile = "frr_" + mode + "_" + errorModelName + ".dat";
      std::ofstream os(dataFile.c_str());

      for (double snr = -5.0; snr <= 30.0; snr += 1.0) {
        uint32_t packetsSent = 0;
        uint32_t packetsReceived = 0;

        for (int run = 0; run < 10; ++run) {
          NodeContainer nodes;
          nodes.Create(2);

          WifiHelper wifi;
          wifi.SetStandard(WIFI_PHY_STANDARD_80211a);

          YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
          YansWifiPhyHelper phy;
          phy.SetChannel(channel.Create());
          phy.SetErrorRateModel(errorModelName);
          phy.Set("Snr", DoubleValue(snr));

          WifiMacHelper mac;
          mac.SetType("ns3::AdhocWifiMac");
          NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

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

          OnOffHelper onoff("ns3::UdpClient", Address(InetSocketAddress(Ipv4Address("127.0.0.1"), 9)));
          onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
          onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
          onoff.SetAttribute("PacketSize", UintegerValue(frameSize));
          onoff.SetAttribute("DataRate", StringValue(mode));

          ApplicationContainer app = onoff.Install(nodes.Get(0));
          app.Start(Seconds(0.1));
          app.Stop(Seconds(simulationTime + 0.1));

          PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), 9));
          ApplicationContainer sinkApp = sink.Install(nodes.Get(1));
          sinkApp.Start(Seconds(0.0));
          sinkApp.Stop(Seconds(simulationTime + 0.2));

          Simulator::Stop(Seconds(simulationTime + 0.2));
          Simulator::Run();

          Ptr<PacketSink> packetSink = DynamicCast<PacketSink>(sinkApp.Get(0));
          packetsReceived += packetSink->GetTotalRx();

          packetsSent += (uint32_t)((simulationTime)* atof(mode.substr(8, mode.size()).erase(mode.substr(8, mode.size()).size()-3).c_str())*125000.0 / (frameSize * 8)); // bits to bytes conversion

          Simulator::Destroy();
        }

        double frr = (double)(packetsSent - packetsReceived) / packetsSent;
        os << snr << " " << frr << std::endl;
      }
      os.close();

      std::string plotFile = "frr_" + mode + "_" + errorModelName + ".plt";
      std::ofstream plotOs(plotFile.c_str());

      plotOs << "set terminal png size 640,480" << std::endl;
      plotOs << "set output \"" << "frr_" + mode + "_" + errorModelName + ".png" << "\"" << std::endl;
      plotOs << "set title \"Frame Error Rate vs SNR (" << mode << ", " << errorModelName << ")\"" << std::endl;
      plotOs << "set xlabel \"SNR (dB)\"" << std::endl;
      plotOs << "set ylabel \"Frame Error Rate\"" << std::endl;
      plotOs << "set grid" << std::endl;
      plotOs << "plot \"" << dataFile << "\" using 1:2 with lines title \"" << errorModelName << "\"" << std::endl;
      plotOs.close();

      std::string command = "gnuplot " + plotFile;
      system(command.c_str());
    }
  }

  return 0;
}