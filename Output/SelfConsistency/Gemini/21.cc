#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/gnuplot.h"
#include <iostream>
#include <fstream>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiClearChannelExperiment");

class Experiment {
public:
    Experiment(double distance, std::string dataRate, std::string phyMode)
        : m_distance(distance), m_dataRate(dataRate), m_phyMode(phyMode), m_packetsReceived(0) {}

    void Run(int simulationTime) {
        CreateNodes();
        InstallWifi();
        InstallInternetStack();
        PositionNodes();
        InstallApplications();

        Simulator::Stop(Seconds(simulationTime));
        Simulator::Run();

        CalculateResults();

        Simulator::Destroy();
    }

    int GetPacketsReceived() const {
        return m_packetsReceived;
    }

private:
    void CreateNodes() {
        m_nodes.Create(2);
    }

    void InstallWifi() {
        WifiHelper wifi;
        wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        YansWifiPhyHelper phy = YansWifiPhyHelper::Create();
        phy.SetChannel(channel.Create());

        wifi.SetRemoteStationManager("ns3::AarfWifiManager");

        WifiMacHelper mac;
        Ssid ssid = Ssid("ns3-wifi");
        mac.SetType("ns3::StaWifiMac",
                    "Ssid", SsidValue(ssid),
                    "ActiveProbing", BooleanValue(false));

        NetDeviceContainer staDevices = wifi.Install(phy, mac, m_nodes.Get(1));

        mac.SetType("ns3::ApWifiMac",
                    "Ssid", SsidValue(ssid));

        NetDeviceContainer apDevices = wifi.Install(phy, mac, m_nodes.Get(0));

        m_devices.Add(staDevices);
        m_devices.Add(apDevices);

        // Set the data rate and PHY mode
        Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/TxPowerStart", DoubleValue(16.0206));
        Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/TxPowerEnd", DoubleValue(16.0206));
        Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/TxPowerLevels", UintegerValue(1));
        Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/Antennas", UintegerValue(1));
        Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ChannelWidth", UintegerValue(22));

        phy.Set("ShortPlcpPreambleSupported", BooleanValue(true));
        phy.Set("TxGain", DoubleValue(10));
        phy.Set("RxGain", DoubleValue(10));
        phy.Set("RxNoiseFigure", DoubleValue(7));
        phy.Set("CcaEdThreshold", DoubleValue(-79));
        phy.Set("EnergyDetectionThreshold", DoubleValue(-79));

        // Set the data rate using the provided dataRate string
        Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/BasicDataRates", StringValue(m_dataRate));
        Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/SupportedDataRates", StringValue(m_dataRate));
        // Set the PHY mode
        phy.Set("PreambleDetectionTimeout", TimeValue(MicroSeconds(1)));
        phy.Set("PLCPHeaderDetectionTimeout", TimeValue(MicroSeconds(1)));
        phy.Set("PlcpPreambleGuardTimeout", TimeValue(MicroSeconds(1)));
        phy.Set("Slot", TimeValue(MicroSeconds(20)));
        phy.Set("Sifs", TimeValue(MicroSeconds(10)));

    }

    void InstallInternetStack() {
        InternetStackHelper stack;
        stack.Install(m_nodes);

        Ipv4AddressHelper address;
        address.SetBase("10.1.1.0", "255.255.255.0");
        m_interfaces = address.Assign(m_devices);
    }

    void PositionNodes() {
        MobilityHelper mobility;
        mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                      "MinX", DoubleValue(0.0),
                                      "MinY", DoubleValue(0.0),
                                      "DeltaX", DoubleValue(m_distance),
                                      "DeltaY", DoubleValue(0.0),
                                      "GridWidth", UintegerValue(1),
                                      "LayoutType", StringValue("RowFirst"));
        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        mobility.Install(m_nodes);
    }

    void InstallApplications() {
        uint16_t port = 9;  // Discard port (RFC 863)

        UdpEchoServerHelper echoServer(port);
        ApplicationContainer serverApps = echoServer.Install(m_nodes.Get(1));
        serverApps.Start(Seconds(0.0));
        serverApps.Stop(Seconds(10.0));

        UdpEchoClientHelper echoClient(m_interfaces.GetAddress(1), port);
        echoClient.SetAttribute("MaxPackets", UintegerValue(100));
        echoClient.SetAttribute("Interval", TimeValue(MilliSeconds(1)));
        echoClient.SetAttribute("PacketSize", UintegerValue(1024));
        ApplicationContainer clientApps = echoClient.Install(m_nodes.Get(0));
        clientApps.Start(Seconds(1.0));
        clientApps.Stop(Seconds(10.0));

        // Trace sink for received packets
        Ptr<Application> app = serverApps.Get(0);
        Ptr<UdpEchoServer> server = DynamicCast<UdpEchoServer>(app);
        server->TraceConnectWithoutContext("Rx", MakeCallback(&Experiment::ReceivePacket, this));

    }

    void CalculateResults() {
        NS_LOG_UNCOND("Packets Received: " << m_packetsReceived);
    }

    void ReceivePacket() {
        m_packetsReceived++;
    }


private:
    NodeContainer m_nodes;
    NetDeviceContainer m_devices;
    Ipv4InterfaceContainer m_interfaces;
    double m_distance;
    std::string m_dataRate;
    std::string m_phyMode;
    int m_packetsReceived;
};


int main(int argc, char* argv[]) {
    LogComponentEnable("WifiClearChannelExperiment", LOG_LEVEL_INFO);

    double distance = 5.0; // Distance between nodes in meters
    int simulationTime = 10; // Simulation time in seconds

    // Configure data rates to test
    std::vector<std::string> dataRates = {
        "DsssRate1Mbps",
        "DsssRate2Mbps",
        "CckRate5_5Mbps",
        "CckRate11Mbps"
    };

    // Configure PHY modes (not really varying it in this example)
    std::string phyMode = "DsssRate1Mbps";

    // Gnuplot output setup
    std::string fileNameWithNoExtension = "wifi-clear-channel";
    std::string graphicsFileName = fileNameWithNoExtension + ".eps";
    std::string plotFileName = fileNameWithNoExtension + ".plt";
    std::string dataFileName = fileNameWithNoExtension + ".dat";

    Gnuplot gnuplot(graphicsFileName);
    gnuplot.SetTitle("WiFi Clear Channel Performance vs. RSS");
    gnuplot.SetLegend("Data Rate", "Packets Received");

    std::ofstream dataFile(dataFileName.c_str());
    dataFile << "# Data Rate   Packets Received" << std::endl;

    for (const auto& dataRate : dataRates) {
        NS_LOG_UNCOND("Running experiment with data rate: " << dataRate);
        Experiment experiment(distance, dataRate, phyMode);
        experiment.Run(simulationTime);
        int packetsReceived = experiment.GetPacketsReceived();

        NS_LOG_UNCOND("Data Rate: " << dataRate << ", Packets Received: " << packetsReceived);

        // Write data to file for Gnuplot
        dataFile << dataRate << " " << packetsReceived << std::endl;
    }

    dataFile.close();

    // Generate Gnuplot script
    gnuplot.AddDataset(dataFileName, "linespoints");
    std::ofstream plotFile(plotFileName.c_str());
    plotFile << gnuplot.GetPlotFile();
    plotFile.close();

    return 0;
}