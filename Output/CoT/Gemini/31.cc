#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/gnuplot.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/command-line.h"
#include "ns3/config-store.h"
#include "ns3/internet-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {

    bool enableAsciiTrace = false;
    bool enablePcap = false;
    std::string phyMode("HeNss-MCS0");
    double simulationTime = 10;
    double distance = 5;
    double minSnr = 0;
    double maxSnr = 30;
    int snrSteps = 31;
    uint32_t packetSize = 1472;

    CommandLine cmd(__FILE__);
    cmd.AddValue("EnableAsciiTrace", "Enable ASCII traces.", enableAsciiTrace);
    cmd.AddValue("EnablePcap", "Enable PCAP traces.", enablePcap);
    cmd.AddValue("phyMode", "Wifi Phy mode", phyMode);
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.AddValue("distance", "Distance between nodes", distance);
    cmd.AddValue("minSnr", "Minimum SNR value", minSnr);
    cmd.AddValue("maxSnr", "Maximum SNR value", maxSnr);
    cmd.AddValue("snrSteps", "Number of SNR steps", snrSteps);
    cmd.AddValue("packetSize", "Size of each packet", packetSize);
    cmd.Parse(argc, argv);

    Config::SetDefault("ns3::WifiMacQueue::MaxSize", StringValue("1000"));

    NodeContainer nodes;
    nodes.Create(2);

    WifiHelper wifiHelper;
    wifiHelper.SetStandard(WIFI_PHY_STANDARD_80211ax);

    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.Set("RxGain", DoubleValue(10));

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    channel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    channel.AddPropagationLoss("ns3::FriisPropagationLossModel");
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns3-ssid");
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevices;
    staDevices = wifiHelper.Install(phy, mac, nodes.Get(1));

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid),
                "BeaconGeneration", BooleanValue(true),
                "BeaconInterval", TimeValue(Seconds(0.1)));

    NetDeviceContainer apDevices;
    apDevices = wifiHelper.Install(phy, mac, nodes.Get(0));

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

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(staDevices);
    address.Assign(apDevices);

    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(simulationTime + 1));

    UdpEchoClientHelper echoClient(interfaces.GetAddress(0), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(4294967295));
    echoClient.SetAttribute("Interval", TimeValue(MicroSeconds(1)));
    echoClient.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer clientApps = echoClient.Install(nodes.Get(1));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(simulationTime));

    if (enableAsciiTrace) {
        AsciiTraceHelper ascii;
        phy.EnableAsciiAll(ascii.CreateFileStream("wifi-nist-yans.tr"));
    }

    if (enablePcap) {
        phy.EnablePcap("wifi-nist-yans", apDevices);
    }

    std::vector<std::string> models = {"NistErrorRateModel", "YansErrorRateModel", "RateErrorModel"};
    std::vector<std::string> mcsValues = {"HeNss-MCS0", "HeNss-MCS1", "HeNss-MCS2", "HeNss-MCS3", "HeNss-MCS4",
                                            "HeNss-MCS5", "HeNss-MCS6", "HeNss-MCS7", "HeNss-MCS8", "HeNss-MCS9",
                                            "HeNss-MCS10", "HeNss-MCS11"};

    for (auto const& mcs : mcsValues) {
        for (auto const& modelName : models) {

            std::string dataFilename = "frame-success-rate-" + mcs + "-" + modelName + ".dat";
            std::ofstream os(dataFilename.c_str());
            os << "# SNR FrameSuccessRate" << std::endl;

            for (int snr = minSnr; snr <= maxSnr; ++snr) {
                Config::Set("/ChannelList/*/$ns3::YansWifiChannel/PropagationLoss/$ns3::FriisPropagationLossModel/Frequency", DoubleValue(5.18e9));
                Config::Set("/ChannelList/*/$ns3::YansWifiChannel/PropagationLoss/$ns3::FriisPropagationLossModel/SystemLoss", DoubleValue(1));
                Config::Set("/ChannelList/*/$ns3::YansWifiChannel/PropagationLoss/$ns3::FriisPropagationLossModel/Lambda", DoubleValue(0.05787));

                Config::SetDefault("ns3::YansWifiPhy::ErrorRateModel", StringValue(modelName));

                if (modelName == "RateErrorModel") {
                    Config::SetDefault("ns3::RateErrorModel::ErrorUnit", StringValue("EU_BIT"));
                    Config::SetDefault("ns3::RateErrorModel::ErrorRate", DoubleValue(0.0));
                    Config::SetDefault("ns3::RateErrorModel::SimulationSeed", IntegerValue(1));

                    Config::Set("/NodeList/1/DeviceList/*/$ns3::WifiNetDevice/Phy/ChannelNumber", UintegerValue(36));
                    Config::Set("/NodeList/1/DeviceList/*/$ns3::WifiNetDevice/Phy/Frequency", DoubleValue(5.18e9));
                    Config::Set("/NodeList/1/DeviceList/*/$ns3::WifiNetDevice/Phy/ChannelWidth", UintegerValue(20));
                    Config::Set("/NodeList/1/DeviceList/*/$ns3::WifiNetDevice/Phy/Standard", EnumValue(WIFI_PHY_STANDARD_80211ax));
                    Config::Set("/NodeList/1/DeviceList/*/$ns3::WifiNetDevice/Phy/ShortGuardEnabled", BooleanValue(false));
                }

                wifiHelper.SetRemoteStationManager("ns3::HeWifiRemoteStationManager",
                                                    "NonUnicastMode", StringValue(mcs),
                                                    "EnableDca", BooleanValue(false));

                double noiseDbm = -95;
                double txPowerDbm = noiseDbm + snr;

                Config::Set("/NodeList/0/DeviceList/*/$ns3::WifiNetDevice/Phy/TxPowerStart", DoubleValue(txPowerDbm));
                Config::Set("/NodeList/0/DeviceList/*/$ns3::WifiNetDevice/Phy/TxPowerEnd", DoubleValue(txPowerDbm));
                Config::Set("/NodeList/1/DeviceList/*/$ns3::WifiNetDevice/Phy/TxPowerStart", DoubleValue(txPowerDbm));
                Config::Set("/NodeList/1/DeviceList/*/$ns3::WifiNetDevice/Phy/TxPowerEnd", DoubleValue(txPowerDbm));

                Simulator::Stop(Seconds(simulationTime));

                FlowMonitorHelper flowMonitor;
                Ptr<FlowMonitor> monitor = flowMonitor.InstallAll();

                Simulator::Run();

                Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowMonitor.GetClassifier());
                FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

                double frameSuccessRate = 0.0;
                for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
                    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
                    if (t.sourceAddress == "10.1.1.2" && t.destinationAddress == "10.1.1.1") {
                        frameSuccessRate = (double)i->second.rxPackets / (double)i->second.txPackets;
                        break;
                    }
                }

                os << snr << " " << frameSuccessRate << std::endl;
                Simulator::Destroy();
            }
            os.close();

            std::string plotFilename = "frame-success-rate-" + mcs + "-" + modelName + ".plt";
            std::ofstream plotFile(plotFilename.c_str());

            plotFile << "set terminal png size 640,480" << std::endl;
            plotFile << "set output \"" << "frame-success-rate-" + mcs + "-" + modelName + ".png" << "\"" << std::endl;
            plotFile << "set title \"Frame Success Rate vs. SNR (" << mcs << ", " << modelName << ")\"" << std::endl;
            plotFile << "set xlabel \"SNR (dB)\"" << std::endl;
            plotFile << "set ylabel \"Frame Success Rate\"" << std::endl;
            plotFile << "set xrange [" << minSnr << ":" << maxSnr << "]" << std::endl;
            plotFile << "set yrange [0:1]" << std::endl;
            plotFile << "plot \"" << dataFilename << "\" using 1:2 with lines title \"" << modelName << "\"" << std::endl;
            plotFile.close();

            std::string command = "gnuplot " + plotFilename;
            system(command.c_str());
        }
    }

    return 0;
}