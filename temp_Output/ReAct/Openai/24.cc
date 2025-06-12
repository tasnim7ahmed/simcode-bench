#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/gnuplot.h"

using namespace ns3;

static void
ComputeFer (
    Ptr<WifiPhy> phy,
    Ptr<ErrorRateModel> errorModel,
    uint64_t frameSize,
    double snrDb,
    WifiMode mode,
    double &fer)
{
    const uint32_t nFrames = 100000;
    uint32_t nErrors = 0;
    double snrLinear = std::pow(10.0, snrDb / 10.0);
    for (uint32_t i = 0; i < nFrames; ++i)
    {
        bool error = false;
        // Each frame is composed of a PHY header and payload
        WifiPreamble preamble = WIFI_PREAMBLE_LONG;
        double per = errorModel->GetChunkSuccessRate(mode, frameSize * 8, snrLinear, preamble);
        double randVal = UniformVariable().GetValue();
        error = (randVal > per);
        if (error)
        {
            ++nErrors;
        }
    }
    fer = static_cast<double> (nErrors) / nFrames;
}

int main(int argc, char *argv[])
{
    uint64_t frameSize = 1200;          // bytes
    uint32_t mcsMin = 0;
    uint32_t mcsMax = 7;
    uint32_t mcsStep = 4;
    double snrStart = 0.0;
    double snrEnd = 30.0;
    double snrStep = 1.0;
    bool enableNist = true;
    bool enableYans = true;
    bool enableTable = true;
    std::string outputPlot = "fer-compare.eps";

    CommandLine cmd(__FILE__);
    cmd.AddValue("frameSize", "Size of the data frame in bytes", frameSize);
    cmd.AddValue("mcsMin", "Minimum MCS index", mcsMin);
    cmd.AddValue("mcsMax", "Maximum MCS index", mcsMax);
    cmd.AddValue("mcsStep", "Step for MCS index", mcsStep);
    cmd.AddValue("snrStart", "Starting SNR (dB)", snrStart);
    cmd.AddValue("snrEnd", "Ending SNR (dB)", snrEnd);
    cmd.AddValue("snrStep", "Step for SNR in dB", snrStep);
    cmd.AddValue("enableNist", "Enable Nist error model", enableNist);
    cmd.AddValue("enableYans", "Enable Yans error model", enableYans);
    cmd.AddValue("enableTable", "Enable Table-based error model", enableTable);
    cmd.AddValue("outputPlot", "Filename for Gnuplot output", outputPlot);
    cmd.Parse(argc, argv);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n);
    WifiMacHelper mac;
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    phy.SetChannel(channel.Create());

    // Set up Gnuplot
    Gnuplot plot(outputPlot);
    plot.SetTitle("Frame Error Rate vs SNR for Nist, Yans, Table-based Error Models");
    plot.SetTerminal("postscript eps color enhanced");
    plot.SetLegend("SNR [dB]", "FER");

    Gnuplot2dDataset datasetNist[3];
    Gnuplot2dDataset datasetYans[3];
    Gnuplot2dDataset datasetTable[3];
    std::string mcsLegend[3];

    uint32_t mcsIndices[3];
    {
        uint32_t i = 0;
        for (uint32_t mcs = mcsMin; mcs <= mcsMax && i < 3; mcs += mcsStep, ++i)
        {
            mcsIndices[i] = mcs;
            std::ostringstream oss;
            oss << "MCS " << mcs;
            mcsLegend[i] = oss.str();
            if(enableNist) datasetNist[i].SetTitle("Nist " + oss.str());
            if(enableYans) datasetYans[i].SetTitle("Yans " + oss.str());
            if(enableTable) datasetTable[i].SetTitle("Table " + oss.str());
        }
    }

    for(uint32_t i = 0; i < 3; ++i)
    {
        if(enableNist) datasetNist[i].SetStyle(Gnuplot2dDataset::LINES);
        if(enableYans) datasetYans[i].SetStyle(Gnuplot2dDataset::LINES);
        if(enableTable) datasetTable[i].SetStyle(Gnuplot2dDataset::LINES);
    }

    for (uint32_t idx = 0; idx < 3; ++idx)
    {
        if (mcsIndices[idx] > mcsMax) continue;
        uint32_t mcs = mcsIndices[idx];

        WifiPhyStandard standard = WIFI_PHY_STANDARD_80211n_5GHZ;
        WifiMode mode;
        Ptr<WifiPhy> dummyPhy = phy.Create<YansWifiPhy>();
        dummyPhy->SetStandard(standard);
        wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
            "DataMode", StringValue("HtMcs" + std::to_string(mcs)),
            "ControlMode", StringValue("HtMcs0"));
        mode = WifiPhyHelper::GetHtMcs(mcs);

        for (double snr = snrStart; snr <= snrEnd; snr += snrStep)
        {
            double ferNist = 0.0, ferYans = 0.0, ferTable = 0.0;

            if (enableNist)
            {
                Ptr<NistErrorRateModel> nistModel = CreateObject<NistErrorRateModel>();
                ComputeFer(dummyPhy, nistModel, frameSize, snr, mode, ferNist);
                datasetNist[idx].Add(snr, ferNist);
            }
            if (enableYans)
            {
                Ptr<YansErrorRateModel> yansModel = CreateObject<YansErrorRateModel>();
                ComputeFer(dummyPhy, yansModel, frameSize, snr, mode, ferYans);
                datasetYans[idx].Add(snr, ferYans);
            }
            if (enableTable)
            {
                Ptr<TableBasedErrorRateModel> tableModel = CreateObject<TableBasedErrorRateModel>();
                ComputeFer(dummyPhy, tableModel, frameSize, snr, mode, ferTable);
                datasetTable[idx].Add(snr, ferTable);
            }
        }
    }
    // Add datasets
    for(uint32_t idx = 0; idx < 3; ++idx)
    {
        if (enableNist)
            plot.AddDataset(datasetNist[idx]);
        if (enableYans)
            plot.AddDataset(datasetYans[idx]);
        if (enableTable)
            plot.AddDataset(datasetTable[idx]);
    }
    // Output the plot
    std::ofstream plotFile(outputPlot);
    plot.GenerateOutput(plotFile);
    plotFile.close();

    return 0;
}