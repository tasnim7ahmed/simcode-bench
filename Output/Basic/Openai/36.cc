#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/config-store-module.h"
#include "ns3/gnuplot.h"
#include "ns3/wifi-module.h"

using namespace ns3;

static std::vector<Ptr<ErrorRateModel>> CreateErrorRateModels()
{
    std::vector<Ptr<ErrorRateModel>> models;

    Ptr<NistErrorRateModel> nist = CreateObject<NistErrorRateModel>();
    models.push_back(nist);

    Ptr<YansErrorRateModel> yans = CreateObject<YansErrorRateModel>();
    models.push_back(yans);

    Ptr<TableBasedErrorRateModel> table = CreateObject<TableBasedErrorRateModel>();
    models.push_back(table);

    return models;
}

struct ModelInfo {
    Ptr<ErrorRateModel> model;
    std::string name;
};

static std::vector<ModelInfo> GetModels()
{
    std::vector<ModelInfo> models;

    models.push_back({ CreateObject<NistErrorRateModel>(), "NIST" });
    models.push_back({ CreateObject<YansErrorRateModel>(), "YANS" });
    models.push_back({ CreateObject<TableBasedErrorRateModel>(), "TABLE" });

    return models;
}

static std::vector<WifiTxVector> GetVhtModes()
{
    std::vector<WifiTxVector> modes;
    uint8_t nss = 1;
    uint8_t mcsMax = 8; // MCS 9 is not valid for 20MHz VHT single-stream, skip MCS9
    WifiTxVector txVector;

    for (uint8_t mcs = 0; mcs <= mcsMax; ++mcs)
    {
        txVector = WifiTxVector();
        txVector.SetMode (WifiPhy::GetVhtMcs (mcs, nss, 0, 20));
        txVector.SetNss (nss);
        txVector.SetChannelWidth (20);
        txVector.SetPreambleType (WIFI_PREAMBLE_VHT);
        txVector.SetGuardInterval (WIFI_GI_800ns);
        txVector.SetBpsk12Validity (true);
        modes.push_back(txVector);
    }
    return modes;
}

int main(int argc, char *argv[])
{
    uint32_t frameSize = 1200;
    CommandLine cmd;
    cmd.AddValue("frameSize", "Frame size in bytes", frameSize);
    cmd.Parse(argc, argv);

    std::vector<ModelInfo> models = GetModels();
    std::vector<WifiTxVector> vhtModes = GetVhtModes();

    double snrStart = -5.0;
    double snrEnd = 30.0;
    double snrStep = 1.0;

    // Prepare gnuplot files
    std::map<std::string, Gnuplot *> plots;
    std::map<std::string, std::vector<Gnuplot2dDataset> > datasets;

    for (const auto &model : models)
    {
        std::string plotTitle = "Frame Success Rate vs SNR (" + model.name + " Error Rate Model)";
        std::string fileName = "fsr_snr_" + model.name + ".plt";
        plots[model.name] = new Gnuplot(fileName, plotTitle);
        plots[model.name]->SetTerminal("png");
        plots[model.name]->SetLegend("SNR (dB)", "Frame Success Rate");
        datasets[model.name].resize(vhtModes.size());
        for (auto &ds : datasets[model.name])
        {
            ds.SetStyle(Gnuplot2dDataset::LINES);
        }
    }

    for (uint32_t i = 0; i < vhtModes.size(); ++i)
    {
        std::ostringstream oss;
        oss << "VHT MCS " << i << " (20MHz)";
        std::string curveName = oss.str();
        for (auto &m : models)
        {
            datasets[m.name][i].SetTitle(curveName);
        }
    }

    // Now, for each mode, compute frame success rate over SNR for each model
    for (uint32_t modeIdx = 0; modeIdx < vhtModes.size(); ++modeIdx)
    {
        WifiTxVector txVector = vhtModes[modeIdx];
        WifiMode mode = txVector.GetMode();

        for (double snr = snrStart; snr <= snrEnd + 1e-6; snr += snrStep) // Ensure endpoint
        {
            double noise = 1.0;
            double signal = noise * std::pow(10.0, snr / 10.0);
            for (auto &model : models)
            {
                double per = model.model->GetChunkSuccessRate(mode,
                                                              txVector,
                                                              signal,
                                                              noise,
                                                              frameSize * 8 * 1.0,  // bits
                                                              frameSize);

                double fsr = 1.0 - per;
                fsr = std::max(0.0, std::min(fsr, 1.0));
                datasets[model.name][modeIdx].Add(snr, fsr);
            }
        }
    }

    for (auto &model : models)
    {
        for (auto &ds : datasets[model.name])
        {
            plots[model.name]->AddDataset(ds);
        }
        plots[model.name]->GenerateOutput(std::cout);
    }

    for (auto &entry : plots)
    {
        delete entry.second;
    }

    return 0;
}