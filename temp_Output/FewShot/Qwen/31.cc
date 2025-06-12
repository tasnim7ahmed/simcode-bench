#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/gnuplot.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ErrorRateModelComparison");

class ErrorRatePlot : public Object
{
public:
    static TypeId GetTypeId(void);
    ErrorRatePlot();
    void Run(uint32_t frameSize, double snrStart, double snrEnd, uint32_t snrSteps);
private:
    void PlotFrameSuccessRate(Gnuplot2dDataset dataset, std::string filename, std::string title);
};

TypeId
ErrorRatePlot::GetTypeId(void)
{
    static TypeId tid = TypeId("ErrorRatePlot")
        .SetParent<Object>()
        .SetGroupName("Wifi");
    return tid;
}

ErrorRatePlot::ErrorRatePlot()
{
}

void
ErrorRatePlot::Run(uint32_t frameSize, double snrStart, double snrEnd, uint32_t snrSteps)
{
    // Setup Gnuplot
    Gnuplot gnuplot;
    gnuplot.SetTitle("Frame Success Rate vs SNR for Different Error Rate Models");
    gnuplot.SetTerminal("png");
    gnuplot.SetLegend("SNR (dB)", "Frame Success Rate");
    gnuplot.AppendExtra("set grid");

    // HT MCS values to iterate over (0 to 7 as example for EHT MCS indices)
    for (uint8_t mcsValue = 0; mcsValue <= 7; ++mcsValue)
    {
        Gnuplot2dDataset datasetNist;
        Gnuplot2dDataset datasetYans;
        Gnuplot2dDataset datasetTable;

        datasetNist.SetStyle(Gnuplot2dDataset::LINES_POINTS);
        datasetYans.SetStyle(Gnuplot2dDataset::LINES_POINTS);
        datasetTable.SetStyle(Gnuplot2dDataset::LINES_POINTS);

        std::ostringstream titleNist, titleYans, titleTable;
        titleNist << "MCS" << static_cast<uint16_t>(mcsValue) << " - Nist";
        titleYans << "MCS" << static_cast<uint16_t>(mcsValue) << " - Yans";
        titleTable << "MCS" << static_cast<uint16_t>(mcsValue) << " - Table";

        datasetNist.SetTitle(titleNist.str());
        datasetYans.SetTitle(titleYans.str());
        datasetTable.SetTitle(titleTable.str());

        double snrStep = (snrEnd - snrStart) / snrSteps;
        for (double snrDb = snrStart; snrDb <= snrEnd; snrDb += snrStep)
        {
            double snrLinear = std::pow(10.0, snrDb / 10.0);

            // Create error rate models
            Ptr<NistErrorRateModel> nist = CreateObject<NistErrorRateModel>();
            Ptr<YansErrorRateModel> yans = CreateObject<YansErrorRateModel>();
            Ptr<TableBasedErrorRateModel> table = CreateObject<TableBasedErrorRateModel>();

            // HT MCS settings for Wi-Fi rates
            WifiMode mode = WifiPhy::GetHtMcs(mcsValue);
            WifiTxVector txVector(mode, 0, WIFI_PREAMBLE_HT_MF, 800, 1, 1, 0, 20, false);

            // Frame success rate calculation
            double fsrcNist = 1.0 - nist->GetChunkSuccessRate(mode, txVector, snrLinear, frameSize * 8);
            double fsrcYans = 1.0 - yans->GetChunkSuccessRate(mode, txVector, snrLinear, frameSize * 8);
            double fsrcTable = 1.0 - table->GetChunkSuccessRate(mode, txVector, snrLinear, frameSize * 8);

            datasetNist.Add(snrDb, fsrcNist);
            datasetYans.Add(snrDb, fsrcYans);
            datasetTable.Add(snrDb, fsrcTable);
        }

        // Generate individual plots per MCS
        std::ostringstream filenameNist, filenameYans, filenameTable;
        filenameNist << "frame-success-rate-mcs" << static_cast<uint16_t>(mcsValue) << "-nist.png";
        filenameYans << "frame-success-rate-mcs" << static_cast<uint16_t>(mcsValue) << "-yans.png";
        filenameTable << "frame-success-rate-mcs" << static_cast<uint16_t>(mcsValue) << "-table.png";

        Gnuplot plotNist;
        plotNist.SetTitle("Frame Success Rate vs SNR - " + titleNist.str());
        plotNist.SetTerminal("png");
        plotNist.SetLegend("SNR (dB)", "Frame Success Rate");
        plotNist.AddDataset(datasetNist);
        plotNist.GenerateOutput(filenameNist.str());

        Gnuplot plotYans;
        plotYans.SetTitle("Frame Success Rate vs SNR - " + titleYans.str());
        plotYans.SetTerminal("png");
        plotYans.SetLegend("SNR (dB)", "Frame Success Rate");
        plotYans.AddDataset(datasetYans);
        plotYans.GenerateOutput(filenameYans.str());

        Gnuplot plotTable;
        plotTable.SetTitle("Frame Success Rate vs SNR - " + titleTable.str());
        plotTable.SetTerminal("png");
        plotTable.SetLegend("SNR (dB)", "Frame Success Rate");
        plotTable.AddDataset(datasetTable);
        plotTable.GenerateOutput(filenameTable.str());
    }
}

void
ErrorRatePlot::PlotFrameSuccessRate(Gnuplot2dDataset dataset, std::string filename, std::string title)
{
    Gnuplot plot;
    plot.SetTitle(title);
    plot.SetTerminal("png");
    plot.SetLegend("SNR (dB)", "Frame Success Rate");
    plot.AddDataset(dataset);
    plot.GenerateOutput(filename);
}

int main(int argc, char *argv[])
{
    uint32_t frameSize = 1500;      // bytes
    double snrStart = 0.0;          // dB
    double snrEnd = 30.0;           // dB
    uint32_t snrSteps = 10;

    CommandLine cmd(__FILE__);
    cmd.AddValue("frameSize", "Size of the frame in bytes", frameSize);
    cmd.AddValue("snrStart", "Start value for SNR sweep in dB", snrStart);
    cmd.AddValue("snrEnd", "End value for SNR sweep in dB", snrEnd);
    cmd.AddValue("snrSteps", "Number of steps for SNR sweep", snrSteps);
    cmd.Parse(argc, argv);

    Ptr<ErrorRatePlot> plotter = CreateObject<ErrorRatePlot>();
    plotter->Run(frameSize, snrStart, snrEnd, snrSteps);

    return 0;
}