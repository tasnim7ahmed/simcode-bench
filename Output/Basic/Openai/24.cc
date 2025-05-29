#include "ns3/core-module.h"
#include "ns3/wifi-module.h"
#include "ns3/gnuplot.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ErrorRateModelComparison");

class ErrorModelComparator
{
public:
  ErrorModelComparator(uint32_t frameSize,
                       std::vector<uint8_t> mcsList,
                       double snrStart,
                       double snrStop,
                       double snrStep,
                       bool useNist,
                       bool useYans,
                       bool useTable)
    : m_frameSize(frameSize),
      m_mcsList(std::move(mcsList)),
      m_snrStart(snrStart),
      m_snrStop(snrStop),
      m_snrStep(snrStep),
      m_useNist(useNist),
      m_useYans(useYans),
      m_useTable(useTable)
  {}

  void Run()
  {
    // Prepare Gnuplot setup
    Gnuplot plot("fer_vs_snr.eps");
    plot.SetTitle("Frame Error Rate vs SNR");
    plot.SetTerminal("postscript eps color enhanced");
    plot.SetLegend("SNR (dB)", "Frame Error Rate (FER)");
    plot.AppendExtra("set logscale y");
    plot.AppendExtra("set xrange [" + std::to_string(m_snrStart) + ":" + std::to_string(m_snrStop) + "]");
    plot.AppendExtra("set yrange [1e-4:1]");

    for (auto mcs : m_mcsList)
    {
      std::vector<Gnuplot2dDataset> datasets;
      if (m_useNist)
      {
        datasets.emplace_back(CreateDataset("Nist MCS " + std::to_string(mcs), "linespoints lt 1 lw 2 pt 6 lc rgb \"#1f77b4\""));
      }
      if (m_useYans)
      {
        datasets.emplace_back(CreateDataset("Yans MCS " + std::to_string(mcs), "linespoints lt 2 lw 2 pt 2 lc rgb \"#d62728\""));
      }
      if (m_useTable)
      {
        datasets.emplace_back(CreateDataset("Table MCS " + std::to_string(mcs), "linespoints lt 3 lw 2 pt 4 lc rgb \"#2ca02c\""));
      }

      for (double snr = m_snrStart; snr <= m_snrStop+1e-9; snr += m_snrStep)
      {
        uint32_t dsidx = 0;
        if (m_useNist)
        {
          double ferNist = CalcFerWithModel("nist", mcs, snr);
          datasets[dsidx++].Add(snr, ferNist);
        }
        if (m_useYans)
        {
          double ferYans = CalcFerWithModel("yans", mcs, snr);
          datasets[dsidx++].Add(snr, ferYans);
        }
        if (m_useTable)
        {
          double ferTable = CalcFerWithModel("table", mcs, snr);
          datasets[dsidx++].Add(snr, ferTable);
        }
      }
      for (auto& ds : datasets)
      {
        plot.AddDataset(ds);
      }
    }

    // Output Gnuplot
    std::ofstream plotFile("fer_vs_snr.plt");
    plot.GenerateOutput(plotFile);
    plotFile.close();
    std::cout << "Plot written to fer_vs_snr.eps, gnuplot script to fer_vs_snr.plt" << std::endl;
  }

private:
  Gnuplot2dDataset CreateDataset(std::string desc, std::string style)
  {
    Gnuplot2dDataset ds;
    ds.SetTitle(desc);
    ds.SetStyle(style);
    return ds;
  }

  double CalcFerWithModel(const std::string& model, uint8_t mcs, double snrDb)
  {
    Ptr<WifiPhy> phy = CreateObject<YansWifiPhy>();
    Ptr<YansWifiChannel> channel = CreateObject<YansWifiChannel>();
    phy->SetChannel(channel);

    Ptr<ErrorRateModel> errModel;

    if (model == "nist")
    {
      errModel = CreateObject<NistErrorRateModel>();
    }
    else if (model == "yans")
    {
      errModel = CreateObject<YansErrorRateModel>();
    }
    else // table
    {
      errModel = CreateObject<TableBasedErrorRateModel>();
    }
    phy->SetErrorRateModel(errModel);

    WifiMode mode = WifiPhyHelper::GetOfdmRate(mcs, WIFI_PHY_STANDARD_80211n_5GHZ);
    double snrLinear = std::pow(10.0, snrDb / 10.0);

    // Assume one frame of frameSize bytes
    double fer = errModel->GetChunkSuccessRate(mode, m_frameSize * 8, snrLinear);

    return 1.0 - fer;
  }

  uint32_t m_frameSize;
  std::vector<uint8_t> m_mcsList;
  double m_snrStart;
  double m_snrStop;
  double m_snrStep;
  bool m_useNist;
  bool m_useYans;
  bool m_useTable;
};

int main(int argc, char *argv[])
{
  uint32_t frameSize = 1200;
  std::string mcsRange = "0,4,7";
  double snrStart = 0.0;
  double snrStop = 30.0;
  double snrStep = 0.5;
  bool enableNist = true;
  bool enableYans = true;
  bool enableTable = true;

  CommandLine cmd;
  cmd.AddValue("frameSize", "Frame size bytes", frameSize);
  cmd.AddValue("mcs", "Comma-separated MCS values (e.g. 0,4,7)", mcsRange);
  cmd.AddValue("snrStart", "Start SNR (dB)", snrStart);
  cmd.AddValue("snrStop", "Stop SNR (dB)", snrStop);
  cmd.AddValue("snrStep", "SNR step (dB)", snrStep);
  cmd.AddValue("enableNist", "Enable NistErrorRateModel", enableNist);
  cmd.AddValue("enableYans", "Enable YansErrorRateModel", enableYans);
  cmd.AddValue("enableTable", "Enable TableBasedErrorRateModel", enableTable);
  cmd.Parse(argc, argv);

  std::vector<uint8_t> mcsList;
  std::istringstream ss(mcsRange);
  std::string s;
  while (std::getline(ss, s, ','))
  {
    mcsList.push_back(static_cast<uint8_t>(std::stoi(s)));
  }

  ErrorModelComparator cmp(frameSize, mcsList, snrStart, snrStop, snrStep, enableNist, enableYans, enableTable);
  cmp.Run();

  return 0;
}