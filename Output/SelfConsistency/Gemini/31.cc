#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/spectrum-module.h"
#include "ns3/propagation-loss-model.h"
#include "ns3/propagation-delay-model.h"
#include "ns3/energy-module.h"
#include "ns3/command-line.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/gnuplot.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"
#include "ns3/nist-error-rate-model.h"
#include "ns3/yans-error-rate-model.h"
#include "ns3/log.h"
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("EhtErrorRateValidation");

int main(int argc, char* argv[]) {
    // Enable logging
    LogComponentEnable("EhtErrorRateValidation", LOG_LEVEL_INFO);

    // Configure simulation parameters
    uint32_t numNodes = 2;
    double simulationTime = 10.0;
    uint32_t packetSize = 1472;
    double minSnr = 0.0;
    double maxSnr = 25.0;
    double snrStep = 1.0;

    // Command line arguments
    CommandLine cmd;
    cmd.AddValue("packetSize", "Size of the packets to send", packetSize);
    cmd.AddValue("simulationTime", "Total duration of the simulation", simulationTime);
    cmd.AddValue("minSnr", "Minimum SNR value", minSnr);
    cmd.AddValue("maxSnr", "Maximum SNR value", maxSnr);
    cmd.AddValue("snrStep", "SNR step", snrStep);

    cmd.Parse(argc, argv);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(numNodes);

    // Configure Wi-Fi
    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211be);

    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();

    // Set up the channel
    YansWifiChannelHelper channel = YansWifiChannelHelper::Simple();
    channel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    channel.AddPropagationLoss("ns3::FriisPropagationLossModel");
    phy.SetChannel(channel.Create());

    // Set error rate models
    NistErrorRateModel nistErrorRateModel;
    YansErrorRateModel yansErrorRateModel;

    phy.SetErrorRateModel(nistErrorRateModel); // Initial model

    WifiMacHelper mac;
    Ssid ssid = Ssid("eht-network");
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevices;
    staDevices = wifi.Install(phy, mac, nodes.Get(1));

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));

    NetDeviceContainer apDevices;
    apDevices = wifi.Install(phy, mac, nodes.Get(0));

    // Mobility
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

    // Internet stack
    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(staDevices);
    interfaces = address.Assign(apDevices);
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Create receiver application on node 1
    uint16_t port = 9;
    UdpServerHelper server(port);
    ApplicationContainer serverApps = server.Install(nodes.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(simulationTime));

    // Define output files
    std::string fileNamePrefix = "eht-error-rate";
    std::string graphicsFileName = fileNamePrefix + ".eps";
    std::string plotFileName = fileNamePrefix + ".plt";
    std::string dataFileNameNist = fileNamePrefix + "-nist.dat";
    std::string dataFileNameYans = fileNamePrefix + "-yans.dat";
    std::string dataFileNameTable = fileNamePrefix + "-table.dat";

    // Iterate through HT MCS values
    for (int mcs = 0; mcs <= 11; ++mcs) {  // Iterate over MCS values
        std::stringstream ss;
        ss << "HeMcs" << mcs;
        std::string rate = ss.str();

        // Open data files for writing
        std::ofstream dataFileNist(dataFileNameNist.c_str(), std::ios::app);
        std::ofstream dataFileYans(dataFileNameYans.c_str(), std::ios::app);
        std::ofstream dataFileTable(dataFileNameTable.c_str(), std::ios::app);

        if (mcs == 0) {
            dataFileNist << "# SNR FrameSuccessRate" << std::endl;
            dataFileYans << "# SNR FrameSuccessRate" << std::endl;
            dataFileTable << "# SNR FrameSuccessRate" << std::endl;
        }

        // Iterate through SNR values
        for (double snr = minSnr; snr <= maxSnr; snr += snrStep) {
            // Configure the sender with the current MCS
            Config::Set("/NodeList/1/DeviceList/*/$ns3::WifiNetDevice/Mac/Minstrel/HeMcs", StringValue(rate));
            // Create OnOff application on node 0
            OnOffHelper clientHelper("ns3::UdpSocketFactory", Address(InetSocketAddress(interfaces.GetAddress(0), port)));
            clientHelper.SetConstantRate(DataRate("50Mbps"), packetSize * 8);
            ApplicationContainer clientApps = clientHelper.Install(nodes.Get(1));
            clientApps.Start(Seconds(2.0));
            clientApps.Stop(Seconds(simulationTime));

            // Set the SNR using a custom propagation loss model
            Ptr<ConstantSpeedPropagationLossModel> lossModel = CreateObject<ConstantSpeedPropagationLossModel>();
            channel.SetPropagationLoss("ns3::ConstantSpeedPropagationLossModel");

            // Simulate with NIST error rate model
            phy.SetErrorRateModel(nistErrorRateModel);
            Simulator::Stop(Seconds(simulationTime));
            Simulator::Run();

            // Calculate frame success rate
            uint32_t totalPacketsSent = DynamicCast<OnOffApplication>(clientApps.Get(0))->GetPacketsSent();
            uint32_t totalPacketsReceived = DynamicCast<UdpServer>(serverApps.Get(0))->GetReceived();

            double frameSuccessRateNist = 0.0;
            if (totalPacketsSent > 0) {
                frameSuccessRateNist = (double)totalPacketsReceived / totalPacketsSent;
            }

            // Write to data file
            dataFileNist << snr << " " << frameSuccessRateNist << std::endl;

            // Simulate with YANS error rate model
            phy.SetErrorRateModel(yansErrorRateModel);
            Simulator::Stop(Seconds(simulationTime));
            Simulator::Run();

            // Calculate frame success rate
            totalPacketsSent = DynamicCast<OnOffApplication>(clientApps.Get(0))->GetPacketsSent();
            totalPacketsReceived = DynamicCast<UdpServer>(serverApps.Get(0))->GetReceived();

            double frameSuccessRateYans = 0.0;
            if (totalPacketsSent > 0) {
                frameSuccessRateYans = (double)totalPacketsReceived / totalPacketsSent;
            }

            // Write to data file
            dataFileYans << snr << " " << frameSuccessRateYans << std::endl;

            // Simulate with Table error rate model
            // Not implemented yet - placeholder values
            double frameSuccessRateTable = 0.0;

            // Write to data file
            dataFileTable << snr << " " << frameSuccessRateTable << std::endl;

             Simulator::Destroy();
        }

        // Close data files
        dataFileNist.close();
        dataFileYans.close();
        dataFileTable.close();
    }

    // Generate Gnuplot script
    Gnuplot plot(graphicsFileName);
    plot.SetTitle("Frame Success Rate vs. SNR (EHT)");
    plot.SetLegend("NIST", "YANS", "Table");
    plot.SetXTitle("SNR (dB)");
    plot.SetYTitle("Frame Success Rate");

    Gnuplot2dDataset datasetNist;
    datasetNist.SetTitle("NIST");
    datasetNist.SetStyle(Gnuplot2dDataset::LINES_POINTS);
    datasetNist.SetFilename(dataFileNameNist);

    Gnuplot2dDataset datasetYans;
    datasetYans.SetTitle("YANS");
    datasetYans.SetStyle(Gnuplot2dDataset::LINES_POINTS);
    datasetYans.SetFilename(dataFileNameYans);

    Gnuplot2dDataset datasetTable;
    datasetTable.SetTitle("Table");
    datasetTable.SetStyle(Gnuplot2dDataset::LINES_POINTS);
    datasetTable.SetFilename(dataFileNameTable);

    plot.AddDataset(datasetNist);
    plot.AddDataset(datasetYans);
    plot.AddDataset(datasetTable);

    std::ofstream plotFile(plotFileName.c_str());
    plot.GenerateOutput(plotFile);
    plotFile.close();

    return 0;
}