#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/gnuplot.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ErrorModelComparison");

class FrameSuccessRateExperiment {
public:
  FrameSuccessRateExperiment(uint32_t frameSize);
  void Run();
private:
  uint32_t m_frameSize;
};

FrameSuccessRateExperiment::FrameSuccessRateExperiment(uint32_t frameSize)
  : m_frameSize(frameSize)
{
}

void
FrameSuccessRateExperiment::Run()
{
  double snrStart = -5.0;
  double snrStop = 30.0;
  double snrStep = 1.0;

  std::vector<std::string> errorModels = {"Yans", "Nist", "Table"};
  WifiPhyStandard phyStandard = WIFI_PHY_STANDARD_80211a;

  Gnuplot gnuplot("frame-success-rate-vs-snr.png");
  gnuplot.SetTitle("Frame Success Rate vs SNR for OFDM Modes");
  gnuplot.SetTerminal("png");
  gnuplot.SetLegend("SNR (dB)", "Frame Success Rate");
  gnuplot.AppendExtra("set style data linespoints");
  gnuplot.AddDataset(Gnuplot2dDataset("NIST"));
  gnuplot.AddDataset(Gnuplot2dDataset("YANS"));
  gnuplot.AddDataset(Gnuplot2dDataset("Table-based"));

  for (uint32_t i = 0; i < errorModels.size(); ++i) {
    std::string em = errorModels[i];
    WifiHelper wifi;
    wifi.SetStandard(phyStandard);

    if (em == "Yans") {
      wifi.SetErrorRateModel("ns3::YansErrorRateModel");
    } else if (em == "Nist") {
      wifi.SetErrorRateModel("ns3::NistErrorRateModel");
    } else if (em == "Table") {
      wifi.SetErrorRateModel("ns3::TableBasedErrorRateModel");
    }

    NodeContainer nodes;
    nodes.Create(2);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    Ptr<YansWifiChannel> wifiChannel = channel.Create();
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode", StringValue("OfdmRate6Mbps"),
                                 "ControlMode", StringValue("OfdmRate6Mbps"));

    NetDeviceContainer devices = wifi.Install(wifiChannel, nodes);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    Ptr<NetDevice> sender = devices.Get(0);
    Ptr<NetDevice> receiver = devices.Get(1);

    Ptr<Packet> packet = Create<Packet>(m_frameSize);
    Simulator::ScheduleNow([this, &wifiChannel, sender, receiver, packet, snrStart, snrStop, snrStep, i, &gnuplot](){
      for (double snrDb = snrStart; snrDb <= snrStop; snrDb += snrStep) {
        double successRate = 0.0;
        uint32_t numRuns = 100;
        for (uint32_t run = 0; run < numRuns; ++run) {
          double snr = pow(10.0, snrDb / 10.0);
          struct WifiSpectrumSignalParameters signalParams;
          signalParams.duration = NanoSeconds(1e6); // dummy duration
          signalParams.preamble = WIFI_PREAMBLE_LONG;
          signalParams.txPowerW = 1.0;
          signalParams.txMode = wifi.GetPhy(sender)->GetMode(0);
          signalParams.txCenterFreq = wifi.GetPhy(sender)->GetFrequency();
          signalParams.txWidth = wifi.GetPhy(sender)->GetChannelWidth();
          signalParams.txNSS = wifi.GetPhy(sender)->GetNumberOfSpatialStreams();

          wifiChannel->AddRxSignal(receiver, packet, signalParams);
          bool succeeded = wifiChannel->IsPacketReceived(receiver, packet, snr);
          if (succeeded) {
            successRate += 1.0;
          }
        }
        successRate /= numRuns;
        gnuplot.GetDataset(i).Add(snrDb, successRate);
      }
    });
  }

  Simulator::Run();
  Simulator::Destroy();

  std::ofstream plotFile("frame-success-rate-vs-snr.plt");
  gnuplot.GenerateOutput(plotFile);
}

int
main(int argc, char *argv[])
{
  uint32_t frameSize = 1472;

  CommandLine cmd(__FILE__);
  cmd.AddValue("frameSize", "The size of each transmitted frame in bytes", frameSize);
  cmd.Parse(argc, argv);

  FrameSuccessRateExperiment experiment(frameSize);
  experiment.Run();

  return 0;
}