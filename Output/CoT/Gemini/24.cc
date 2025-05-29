#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/spectrum-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/gnuplot.h"
#include <iostream>
#include <fstream>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("ErrorRateComparison");

int main (int argc, char *argv[])
{
  bool useNist = true;
  bool useYans = true;
  bool useTable = true;
  uint32_t frameSize = 1000;
  uint32_t minMcs = 0;
  uint32_t maxMcs = 7;
  double snrStep = 0.5;
  double minSnr = 5;
  double maxSnr = 25;

  CommandLine cmd;
  cmd.AddValue ("useNist", "Enable Nist error model", useNist);
  cmd.AddValue ("useYans", "Enable Yans error model", useYans);
  cmd.AddValue ("useTable", "Enable Table error model", useTable);
  cmd.AddValue ("frameSize", "Frame size in bytes", frameSize);
  cmd.AddValue ("minMcs", "Minimum MCS value", minMcs);
  cmd.AddValue ("maxMcs", "Maximum MCS value", maxMcs);
  cmd.AddValue ("snrStep", "SNR step size", snrStep);
  cmd.AddValue ("minSnr", "Minimum SNR value", minSnr);
  cmd.AddValue ("maxSnr", "Maximum SNR value", maxSnr);
  cmd.Parse (argc, argv);

  std::map<std::string, std::map<uint32_t, std::vector<double>>> ferData;

  for (uint32_t mcs = minMcs; mcs <= maxMcs; ++mcs)
  {
    ferData["Nist"][mcs] = std::vector<double>();
    ferData["Yans"][mcs] = std::vector<double>();
    ferData["Table"][mcs] = std::vector<double>();

    for (double snr = minSnr; snr <= maxSnr; snr += snrStep)
    {
      double ber = 0.0;
      double per = 0.0;

      if (useNist)
      {
        NistErrorRateModel errorModel;
        errorModel.SetStream (0);
        ber = errorModel.GetBitErrorRate (snr, mcs);
        per = 1 - pow ((1 - ber), frameSize * 8);
        ferData["Nist"][mcs].push_back (per);
      }
      else
      {
        ferData["Nist"][mcs].push_back(0.0);
      }

      if (useYans)
      {
        YansErrorRateModel errorModel;
        errorModel.SetStream (0);
        ber = errorModel.GetBitErrorRate (snr, mcs);
        per = 1 - pow ((1 - ber), frameSize * 8);
        ferData["Yans"][mcs].push_back (per);
      }
      else
      {
        ferData["Yans"][mcs].push_back(0.0);
      }

      if (useTable)
      {
        TableErrorRateModel errorModel;
        errorModel.SetStream (0);
        ber = errorModel.GetBitErrorRate (snr, mcs);
        per = 1 - pow ((1 - ber), frameSize * 8);
        ferData["Table"][mcs].push_back (per);
      }
      else
      {
        ferData["Table"][mcs].push_back(0.0);
      }
    }
  }

  Gnuplot plot ("error_rate_comparison.eps");
  plot.SetTitle ("FER vs SNR for Different Error Models and MCS");
  plot.SetLegend ("Error Model", "FER");
  plot.SetTerminal ("postscript eps color");

  Gnuplot2dDataset datasetNist0;
  Gnuplot2dDataset datasetNist4;
  Gnuplot2dDataset datasetNist7;
  Gnuplot2dDataset datasetYans0;
  Gnuplot2dDataset datasetYans4;
  Gnuplot2dDataset datasetYans7;
  Gnuplot2dDataset datasetTable0;
  Gnuplot2dDataset datasetTable4;
  Gnuplot2dDataset datasetTable7;

  if (useNist)
  {
    double snr = minSnr;
    for (double fer : ferData["Nist"][0])
    {
        datasetNist0.Add (snr, fer);
        snr += snrStep;
    }
    snr = minSnr;
    for (double fer : ferData["Nist"][4])
    {
        datasetNist4.Add (snr, fer);
        snr += snrStep;
    }
    snr = minSnr;
    for (double fer : ferData["Nist"][7])
    {
        datasetNist7.Add (snr, fer);
        snr += snrStep;
    }

    datasetNist0.SetTitle ("Nist MCS 0");
    datasetNist4.SetTitle ("Nist MCS 4");
    datasetNist7.SetTitle ("Nist MCS 7");

    plot.AddDataset (datasetNist0);
    plot.AddDataset (datasetNist4);
    plot.AddDataset (datasetNist7);
  }

  if (useYans)
  {
    double snr = minSnr;
    for (double fer : ferData["Yans"][0])
    {
        datasetYans0.Add (snr, fer);
        snr += snrStep;
    }
    snr = minSnr;
    for (double fer : ferData["Yans"][4])
    {
        datasetYans4.Add (snr, fer);
        snr += snrStep;
    }
    snr = minSnr;
    for (double fer : ferData["Yans"][7])
    {
        datasetYans7.Add (snr, fer);
        snr += snrStep;
    }

    datasetYans0.SetTitle ("Yans MCS 0");
    datasetYans4.SetTitle ("Yans MCS 4");
    datasetYans7.SetTitle ("Yans MCS 7");

    plot.AddDataset (datasetYans0);
    plot.AddDataset (datasetYans4);
    plot.AddDataset (datasetYans7);
  }

  if (useTable)
  {
    double snr = minSnr;
    for (double fer : ferData["Table"][0])
    {
        datasetTable0.Add (snr, fer);
        snr += snrStep;
    }
    snr = minSnr;
    for (double fer : ferData["Table"][4])
    {
        datasetTable4.Add (snr, fer);
        snr += snrStep;
    }
    snr = minSnr;
    for (double fer : ferData["Table"][7])
    {
        datasetTable7.Add (snr, fer);
        snr += snrStep;
    }

    datasetTable0.SetTitle ("Table MCS 0");
    datasetTable4.SetTitle ("Table MCS 4");
    datasetTable7.SetTitle ("Table MCS 7");

    plot.AddDataset (datasetTable0);
    plot.AddDataset (datasetTable4);
    plot.AddDataset (datasetTable7);
  }

  plot.GenerateOutput();

  return 0;
}