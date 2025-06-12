#include "ns3/core-module.h"
#include "ns3/gnuplot.h"
#include "ns3/wifi-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("HeFrameSuccessRatePlot");

class HeFsrPlot {
public:
    HeFsrPlot(uint32_t frameSize);
    void RunSimulation();
private:
    void RunForErrorModel(Ptr<YansErrorRateModel> errorModel, const std::string& modelName);
    uint32_t m_frameSize;
};

HeFsrPlot::HeFsrPlot(uint32_t frameSize)
    : m_frameSize(frameSize) {}

void HeFsrPlot::RunSimulation() {
    // Define SNR values to simulate (in dB)
    std::vector<double> snrDbValues;
    for (double snr = 0; snr <= 30; snr += 2) {
        snrDbValues.push_back(snr);
    }

    // Define output files and plots
    Gnuplot gnuplot("he-fsr-plot.png");
    gnuplot.SetTitle("Frame Success Rate vs SNR for HE Rates");
    gnuplot.SetTerminal("png size 1024,768 enhanced font 'Verdana,12'");
    gnuplot.SetLegend("SNR (dB)", "Frame Success Rate");
    gnuplot.AppendExtra("set grid");

    Gnuplot2dDataset datasetNist;
    datasetNist.SetTitle("NIST");
    datasetNist.SetStyle(Gnuplot2dDataset::LINES_POINTS);

    Gnuplot2dDataset datasetYans;
    datasetYans.SetTitle("YANS");
    datasetYans.SetStyle(Gnuplot2dDataset::LINES_POINTS);

    Gnuplot2dDataset datasetTable;
    datasetTable.SetTitle("Table-based");
    datasetTable.SetStyle(Gnuplot2dDataset::LINES_POINTS);

    // NIST model
    Ptr<NistErrorRateModel> nistModel = CreateObject<NistErrorRateModel>();
    RunForErrorModel(nistModel, "NIST");

    // YANS model
    Ptr<YansErrorRateModel> yansModel = CreateObject<YansErrorRateModel>();
    RunForErrorModel(yansModel, "YANS");

    // Table-based model
    Ptr<TableBasedErrorRateModel> tableModel = CreateObject<TableBasedErrorRateModel>();
    RunForErrorModel(tableModel, "Table-based");

    // Output datasets
    for (double snrDb : snrDbValues) {
        double fsrNist = 0.0, fsrYans = 0.0, fsrTable = 0.0;

        // Recompute FSR for each model
        for (uint8_t mcs = 0; mcs <= 11; ++mcs) { // HE MCS from 0 to 11
            WifiMode mode = WifiPhy::GetHeMcs(mcs);
            double successProbNist = nistModel->GetChunkSuccessRate(mode, Seconds(1), std::pow(10, snrDb / 10), m_frameSize * 8);
            double successProbYans = yansModel->GetChunkSuccessRate(mode, Seconds(1), std::pow(10, snrDb / 10), m_frameSize * 8);
            double successProbTable = tableModel->GetChunkSuccessRate(mode, Seconds(1), std::pow(10, snrDb / 10), m_frameSize * 8);

            fsrNist += successProbNist;
            fsrYans += successProbYans;
            fsrTable += successProbTable;
        }

        fsrNist /= 12;
        fsrYans /= 12;
        fsrTable /= 12;

        datasetNist.Add(snrDb, fsrNist);
        datasetYans.Add(snrDb, fsrYans);
        datasetTable.Add(snrDb, fsrTable);
    }

    gnuplot.AddDataset(datasetNist);
    gnuplot.AddDataset(datasetYans);
    gnuplot.AddDataset(datasetTable);

    std::ofstream plotFile("he_fsr.plot");
    gnuplot.GenerateOutput(plotFile);
    plotFile.close();

    std::cout << "Generated Gnuplot file: he_fsr.plot" << std::endl;
}

void HeFsrPlot::RunForErrorModel(Ptr<YansErrorRateModel> errorModel, const std::string& modelName) {
    std::ostringstream filename;
    filename << "he_fsr_" << modelName << ".dat";
    std::ofstream outFile(filename.str());

    outFile << "# SNR(dB)\tAvg Frame Success Rate\n";

    for (double snrDb = 0; snrDb <= 30; snrDb += 2) {
        double avgFsr = 0.0;
        for (uint8_t mcs = 0; mcs <= 11; ++mcs) {
            WifiMode mode = WifiPhy::GetHeMcs(mcs);
            double successProb = errorModel->GetChunkSuccessRate(mode, Seconds(1), std::pow(10, snrDb / 10), m_frameSize * 8);
            avgFsr += successProb;
        }
        avgFsr /= 12;
        outFile << snrDb << "\t" << avgFsr << "\n";
    }

    outFile.close();
    std::cout << "Generated data file for " << modelName << ": " << filename.str() << std::endl;
}

int main(int argc, char *argv[]) {
    uint32_t frameSize = 1500; // Default frame size in bytes

    CommandLine cmd(__FILE__);
    cmd.AddValue("frameSize", "The size of the frame in bytes", frameSize);
    cmd.Parse(argc, argv);

    HeFsrPlot plotter(frameSize);
    plotter.RunSimulation();

    return 0;
}