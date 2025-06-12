#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/gnuplot.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WiFiSignalStrengthExperiment");

class Experiment
{
public:
    Experiment();
    ~Experiment();

    void Setup(uint32_t run, uint32_t nodeDistance);
    void StartApplications();
    void OnPacketReceive(Ptr<const Packet> packet);
    void PrintRssVsReceivedPacketsGnuplot(std::string fileName);

private:
    NodeContainer m_apNode;
    NodeContainer m_staNode;
    NetDeviceContainer m_apDevice;
    NetDeviceContainer m_staDevice;
    Ipv4InterfaceContainer m_apInterface;
    Ipv4InterfaceContainer m_staInterface;
    uint32_t m_receivedPackets;
};

Experiment::Experiment()
{
    m_receivedPackets = 0;
}

Experiment::~Experiment()
{
}

void Experiment::Setup(uint32_t run, uint32_t nodeDistance)
{
    // Create nodes
    m_apNode.Create(1);
    m_staNode.Create(1);

    // Setup WiFi
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);

    YansWifiPhyHelper phy;
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    phy.SetChannel(channel.Create());

    NqosWifiMacHelper mac = NqosWifiMacHelper::Default();
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode", StringValue("DsssRate1Mbps"),
                                 "ControlMode", StringValue("DsssRate1Mbps"));

    m_staDevice = wifi.Install(phy, mac, m_staNode);
    m_apDevice = wifi.Install(phy, mac, m_apNode);

    // Mobility setup
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(nodeDistance),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(1),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(m_apNode);
    mobility.Install(m_staNode);

    // Internet stack
    InternetStackHelper internet;
    internet.Install(m_apNode);
    internet.Install(m_staNode);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("192.168.1.0", "255.255.255.0");
    m_apInterface = ipv4.Assign(m_apDevice);
    m_staInterface = ipv4.Assign(m_staDevice);

    // Setup UDP server and client
    UdpServerHelper server(9);
    ApplicationContainer serverApp = server.Install(m_staNode.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    UdpClientHelper client(m_staInterface.GetAddress(0), 9);
    client.SetAttribute("MaxPackets", UintegerValue(1000));
    client.SetAttribute("Interval", TimeValue(Seconds(0.1)));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = client.Install(m_apNode.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Connect packet receive callback
    Config::Connect("/NodeList/*/ApplicationList/*/$ns3::UdpServer/Rx", MakeCallback(&Experiment::OnPacketReceive, this));
}

void Experiment::StartApplications()
{
    Simulator::Run();
    Simulator::Destroy();
}

void Experiment::OnPacketReceive(Ptr<const Packet> packet)
{
    m_receivedPackets++;
}

void Experiment::PrintRssVsReceivedPacketsGnuplot(std::string fileName)
{
    Gnuplot gnuplot(fileName + ".eps");
    gnuplot.SetTitle("RSS vs Packets Received");
    gnuplot.SetTerminal("postscript eps color enhanced");
    gnuplot.SetLegend("Received Signal Strength (dBm)", "Packets Received");

    Gnuplot2dDataset dataset;
    dataset.SetTitle("Measured Data");
    dataset.SetStyle(Gnuplot2dDataset::LINES_POINTS);

    Ptr<WifiPhy> phy = m_staNode.Get(0)->GetDevice(0)->GetObject<WifiNetDevice>()->GetPhy();
    double rss = phy->GetCurrentReceivingPowerW() / 1e-3; // Convert W to dBm
    rss = 10 * std::log10(rss);

    dataset.Add(rss, m_receivedPackets);

    gnuplot.AddDataset(dataset);

    std::ofstream plotFile(fileName + ".plt");
    gnuplot.GenerateOutput(plotFile);
    plotFile.close();
}

int main(int argc, char* argv[])
{
    LogComponentEnable("WiFiSignalStrengthExperiment", LOG_LEVEL_INFO);

    std::vector<uint32_t> distances = {10, 20, 30, 40, 50};
    std::vector<uint32_t> runs = {1, 2, 3, 4, 5};

    for (uint32_t run : runs)
    {
        for (uint32_t distance : distances)
        {
            RngSeedManager::SetRun(run);

            Experiment experiment;
            experiment.Setup(run, distance);
            experiment.StartApplications();

            std::ostringstream oss;
            oss << "run-" << run << "-distance-" << distance;
            experiment.PrintRssVsReceivedPacketsGnuplot(oss.str());
        }
    }

    return 0;
}