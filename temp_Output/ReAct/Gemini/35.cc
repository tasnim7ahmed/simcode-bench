#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/gnuplot.h"
#include "ns3/command-line.h"
#include "ns3/yans-wifi-phy.h"
#include "ns3/nist-error-rate-model.h"
#include "ns3/table-based-error-rate-model.h"
#include "ns3/ofdm-utils.h"

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <iomanip>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiErrorModelComparison");

int main(int argc, char *argv[]) {
    bool verbose = false;
    uint32_t packetSize = 1472;
    double simulationTime = 1.0;
    double distance = 1.0;
    std::string rateInfoFilename = "rates.dat";

    CommandLine cmd;
    cmd.AddValue("packetSize", "Size of packet generated. Default 1472", packetSize);
    cmd.AddValue("simulationTime", "Simulation time in seconds. Default 1", simulationTime);
    cmd.AddValue("distance", "Distance between nodes. Default 1", distance);
    cmd.AddValue("verbose", "Tell echo applications to log if true", verbose);
    cmd.AddValue("rateInfoFilename", "Filename containing the rate information to use with the TableBasedErrorRateModel.", rateInfoFilename);
    cmd.Parse(argc, argv);

    if (verbose) {
        LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
        LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    }

    NodeContainer nodes;
    nodes.Create(2);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211n);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    wifiChannel.AddPropagationLoss("ns3::FriisPropagationLossModel");
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiMacHelper wifiMac;
    Ssid ssid = Ssid("ns-3-ssid");
    wifiMac.SetType("ns3::StaWifiMac",
                   "Ssid", SsidValue(ssid),
                   "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevices;
    staDevices = wifi.Install(wifiPhy, wifiMac, nodes.Get(1));

    wifiMac.SetType("ns3::ApWifiMac",
                   "Ssid", SsidValue(ssid));

    NetDeviceContainer apDevices;
    apDevices = wifi.Install(wifiPhy, wifiMac, nodes.Get(0));

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(distance),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(staDevices);
    interfaces = address.Assign(apDevices);

    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(0));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(simulationTime + 1));

    UdpEchoClientHelper echoClient(interfaces.GetAddress(1), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(0xffffffff));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(0.0001)));
    echoClient.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer clientApps = echoClient.Install(nodes.Get(1));
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(simulationTime + 1));

    std::vector<std::string> errorModels = {"YansErrorRateModel", "NistErrorRateModel", "TableBasedErrorRateModel"};
    std::vector<std::string> ofdmModes = {"OfdmRate6Mbps", "OfdmRate12Mbps", "OfdmRate24Mbps"};

    Gnuplot2dDataset datasetYans[ofdmModes.size()];
    Gnuplot2dDataset datasetNist[ofdmModes.size()];
    Gnuplot2dDataset datasetTable[ofdmModes.size()];

    for (size_t i = 0; i < ofdmModes.size(); ++i) {
        datasetYans[i].SetStyle(Gnuplot2dDataset::LINES);
        datasetYans[i].SetTitle(ofdmModes[i] + " YANS");

        datasetNist[i].SetStyle(Gnuplot2dDataset::LINES);
        datasetNist[i].SetTitle(ofdmModes[i] + " NIST");

        datasetTable[i].SetStyle(Gnuplot2dDataset::LINES);
        datasetTable[i].SetTitle(ofdmModes[i] + " Table");
    }

    for (double snr = -5; snr <= 30; snr += 5) {
        double noiseDbm = -100.0;
        double txPowerDbm = noiseDbm + snr;

        Config::Set("/NodeList/0/DeviceList/*/$ns3::WifiNetDevice/Phy/TxPowerStart", DoubleValue(txPowerDbm));
        Config::Set("/NodeList/0/DeviceList/*/$ns3::WifiNetDevice/Phy/TxPowerEnd", DoubleValue(txPowerDbm));
        Config::Set("/NodeList/1/DeviceList/*/$ns3::WifiNetDevice/Phy/TxPowerStart", DoubleValue(txPowerDbm));
        Config::Set("/NodeList/1/DeviceList/*/$ns3::WifiNetDevice/Phy/TxPowerEnd", DoubleValue(txPowerDbm));

        for (size_t i = 0; i < ofdmModes.size(); ++i) {
            Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/NonErpSupported", BooleanValue(false));
            Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/ErpSupported", BooleanValue(false));
            Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/HtSupported", BooleanValue(false));
            Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/VhtSupported", BooleanValue(false));
            Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/HeSupported", BooleanValue(false));
            Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/DataMode", StringValue(ofdmModes[i]));

            Config::Set("/ChannelList/0/PropagationLossModel/ReferenceLoss", DoubleValue(10));
            Config::Set("/ChannelList/0/PropagationLossModel/PathLossExponent", DoubleValue(2));
            Config::Set("/ChannelList/0/PropagationLossModel/Frequency", DoubleValue(2.4e9));

            Config::Set("/ChannelList/0/ChannelConditionModel", TypeIdValue(TypeId::LookupByName("ns3::YansErrorRateModel")));

            Simulator::Run(Seconds(simulationTime));
            uint32_t packetsSent = DynamicCast<UdpEchoClient>(clientApps.Get(0))->GetLost();
            uint32_t packetsReceived = DynamicCast<UdpEchoServer>(serverApps.Get(0))->GetReceived();
            double frameSuccessRate = (double)packetsReceived / DynamicCast<UdpEchoClient>(clientApps.Get(0))->GetNPackets();
            datasetYans[i].Add(snr, frameSuccessRate);
            Simulator::Stop(Seconds(0.0));
            Simulator::Destroy();

            Config::Set("/ChannelList/0/ChannelConditionModel", TypeIdValue(TypeId::LookupByName("ns3::NistErrorRateModel")));
            Simulator::Run(Seconds(simulationTime));
            packetsSent = DynamicCast<UdpEchoClient>(clientApps.Get(0))->GetLost();
            packetsReceived = DynamicCast<UdpEchoServer>(serverApps.Get(0))->GetReceived();
            frameSuccessRate = (double)packetsReceived / DynamicCast<UdpEchoClient>(clientApps.Get(0))->GetNPackets();
            datasetNist[i].Add(snr, frameSuccessRate);
            Simulator::Stop(Seconds(0.0));
            Simulator::Destroy();

            Config::Set("/ChannelList/0/ChannelConditionModel", TypeIdValue(TypeId::LookupByName("ns3::TableBasedErrorRateModel")));
            Config::Set("/ChannelList/0/ChannelConditionModel/FileName", StringValue(rateInfoFilename));
            Simulator::Run(Seconds(simulationTime));
            packetsSent = DynamicCast<UdpEchoClient>(clientApps.Get(0))->GetLost();
            packetsReceived = DynamicCast<UdpEchoServer>(serverApps.Get(0))->GetReceived();
            frameSuccessRate = (double)packetsReceived / DynamicCast<UdpEchoClient>(clientApps.Get(0))->GetNPackets();
            datasetTable[i].Add(snr, frameSuccessRate);
            Simulator::Stop(Seconds(0.0));
            Simulator::Destroy();

            serverApps = echoServer.Install(nodes.Get(0));
            serverApps.Start(Seconds(0.0));
            serverApps.Stop(Seconds(simulationTime + 1));

            clientApps = echoClient.Install(nodes.Get(1));
            clientApps.Start(Seconds(1.0));
            clientApps.Stop(Seconds(simulationTime + 1));
        }
    }

    Gnuplot plot("frame_success_rate.eps");
    plot.SetTitle("Frame Success Rate vs SNR");
    plot.SetLegend("SNR (dB)", "Frame Success Rate");

    for (size_t i = 0; i < ofdmModes.size(); ++i) {
        plot.AddDataset(datasetYans[i]);
        plot.AddDataset(datasetNist[i]);
        plot.AddDataset(datasetTable[i]);
    }
    std::ofstream plotFile("frame_success_rate.plt");
    plot.GenerateOutput(plotFile);
    plotFile.close();

    return 0;
}