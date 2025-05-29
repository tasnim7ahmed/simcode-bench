#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/gnuplot.h"
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>

using namespace ns3;

static std::vector<double> GetVhtRates()
{
    WifiHelper wifi;
    WifiPhyHelper phy = WifiPhyHelper::Default();
    YansWifiPhyHelper yansPhy;
    std::vector<double> rates;
    // 20 MHz VHT MCS 0-8
    Ptr<YansWifiChannel> channel = CreateObject<YansWifiChannel>();
    phy.SetChannel(channel);
    wifi.SetStandard(WIFI_STANDARD_80211ac);
    WifiMacHelper mac;
    Ssid ssid = Ssid("vht-test");
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));
    NetDeviceContainer devs;
    NodeContainer nodes;
    nodes.Create(1);

    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                "DataMode", StringValue("VhtMcs0"),
                                "ControlMode", StringValue("VhtMcs0"));
    devs = wifi.Install(phy, mac, nodes);

    Ptr<WifiNetDevice> wifiDev = DynamicCast<WifiNetDevice>(devs.Get(0));

    for (uint8_t mcs = 0; mcs <= 8; ++mcs)
    {
        std::ostringstream oss;
        oss << "VhtMcs" << unsigned(mcs);
        WifiMode mode = WifiMode(oss.str());
        rates.push_back(wifiDev->GetPhy()->GetDataRate(mode));
    }
    return rates;
}

static std::vector<WifiMode> GetVhtModes()
{
    std::vector<WifiMode> modes;
    WifiPhyHelper phy = WifiPhyHelper::Default();
    YansWifiPhyHelper yansPhy;
    phy.SetChannel(CreateObject<YansWifiChannel>());
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211ac);
    for (uint8_t mcs = 0; mcs <= 8; ++mcs)
    {
        std::ostringstream oss;
        oss << "VhtMcs" << unsigned(mcs);
        modes.push_back(WifiMode(oss.str()));
    }
    return modes;
}

double DbToRatio(double db)
{
    return std::pow(10.0, db / 10.0);
}

int main(int argc, char *argv[])
{
    // Command-line parameters
    uint32_t frameSize = 1200;
    CommandLine cmd;
    cmd.AddValue("frameSize", "Frame size in bytes", frameSize);
    cmd.Parse(argc, argv);

    std::vector<WifiMode> vhtModes = GetVhtModes();
    std::vector<std::string> modelNames = {"Nist", "Yans", "Table"};
    std::vector<Ptr<ErrorRateModel>> models;

    models.push_back(CreateObject<NistErrorRateModel>());
    models.push_back(CreateObject<YansErrorRateModel>());
    models.push_back(CreateObject<TableBasedErrorRateModel>());

    std::vector<Gnuplot> plots;
    plots.reserve(modelNames.size());

    for (const auto& modelName : modelNames)
    {
        Gnuplot plot(modelName + "-vht-fsr-vs-snr.eps");
        plot.SetTitle("Frame Success Rate vs SNR (" + modelName + " Error Rate Model, VHT, Frame " + std::to_string(frameSize) + " bytes)");
        plot.SetTerminal("postscript eps enhanced color font 'Helvetica,10'");
        plot.SetLegend("SNR (dB)", "Frame Success Rate");
        plots.push_back(plot);
    }

    // SNR range -5 to 30 dB
    const double snrBeginDb = -5.0;
    const double snrEndDb = 30.0;
    const double snrStepDb = 1.0;

    for (size_t m = 0; m < vhtModes.size(); ++m)
    {
        std::ostringstream mcsLabel;
        mcsLabel << "MCS " << m;
        std::vector<Gnuplot2dDataset> datasets(modelNames.size());

        for (size_t modelIdx = 0; modelIdx < modelNames.size(); ++modelIdx)
        {
            datasets[modelIdx].SetTitle(mcsLabel.str());
            datasets[modelIdx].SetStyle(Gnuplot2dDataset::LINES);
        }

        for (double snrDb = snrBeginDb; snrDb <= snrEndDb; snrDb += snrStepDb)
        {
            double snr = DbToRatio(snrDb);
            for (size_t modelIdx = 0; modelIdx < modelNames.size(); ++modelIdx)
            {
                Ptr<ErrorRateModel> model = models[modelIdx];

                double per = model->GetChunkSuccessRate(vhtModes[m], snr, frameSize * 8);
                double fsr = per;
                // The method returns 'success rate' directly (probability packet is correct)
                datasets[modelIdx].Add(snrDb, fsr);
            }
        }

        for (size_t modelIdx = 0; modelIdx < modelNames.size(); ++modelIdx)
        {
            plots[modelIdx].AddDataset(datasets[modelIdx]);
        }
    }

    // Write Gnuplot output to files
    for (size_t i = 0; i < plots.size(); ++i)
    {
        std::string dataFile = modelNames[i] + "-vht-fsr-vs-snr.dat";
        std::ofstream outfile(dataFile);
        plots[i].GenerateOutput(outfile);
        outfile.close();
    }

    return 0;
}