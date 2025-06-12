#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/gnuplot.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiRateControlComparison");

class WifiSimulation {
public:
    WifiSimulation(std::string rateManager, uint32_t rtsThreshold, std::string throughputFile, std::string powerFile);
    void Run();

private:
    void SetupNodes();
    void SetupWifi();
    void SetupMobility();
    void SetupApplications();
    void LogRateAndPower(Ptr<OutputStreamWrapper> stream, Time interval);
    void UpdatePosition();

    NodeContainer m_nodes;
    NetDeviceContainer m_apDevice;
    NetDeviceContainer m_staDevice;
    Ipv4InterfaceContainer m_apInterface;
    Ipv4InterfaceContainer m_staInterface;
    uint32_t m_rtsThreshold;
    std::string m_rateManager;
    std::string m_throughputFile;
    std::string m_powerFile;
    Ptr<OutputStreamWrapper> m_powerStream;
    uint32_t m_position;
};

WifiSimulation::WifiSimulation(std::string rateManager, uint32_t rtsThreshold,
                               std::string throughputFile, std::string powerFile)
    : m_rtsThreshold(rtsThreshold),
      m_rateManager(rateManager),
      m_throughputFile(throughputFile),
      m_powerFile(powerFile),
      m_position(0) {}

void WifiSimulation::Run() {
    SetupNodes();
    SetupWifi();
    SetupMobility();
    SetupApplications();

    AsciiTraceHelper asciiTraceHelper;
    m_apDevice = m_nodes.Get(0)->GetDevice(0)->GetObject<WifiNetDevice>();
    m_staDevice = m_nodes.Get(1)->GetDevice(0)->GetObject<WifiNetDevice>();

    m_powerStream = asciiTraceHelper.CreateFileStream(m_powerFile);
    *m_powerStream->GetStream() << "Time(s)\tDistance(m)\tTxPower(dBm)\tDataRate(Mbps)" << std::endl;

    Simulator::Schedule(Seconds(0.5), &WifiSimulation::LogRateAndPower, this, m_powerStream, Seconds(0.5));
    Simulator::Schedule(Seconds(1.0), &WifiSimulation::UpdatePosition, this);

    Simulator::Stop(Seconds(20.0));
    Simulator::Run();
    Simulator::Destroy();
}

void WifiSimulation::SetupNodes() {
    m_nodes.Create(2);
}

void WifiSimulation::SetupWifi() {
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211a);
    wifi.SetRemoteStationManager("ns3::" + m_rateManager, "RtsCtsThreshold", UintegerValue(m_rtsThreshold));

    WifiMacHelper mac;
    Ssid ssid = Ssid("wifi-rate-comparison");

    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    m_apDevice = wifi.Install(mac, m_nodes.Get(0));

    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));
    m_staDevice = wifi.Install(mac, m_nodes.Get(1));
}

void WifiSimulation::SetupMobility() {
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(0.0),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(1),
                                  "LayoutType", StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(m_nodes.Get(0)); // AP fixed at (0, 0)

    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.Install(m_nodes.Get(1)); // STA will move in X direction

    m_nodes.Get(1)->GetObject<ConstantVelocityMobilityModel>()->SetVelocity(Vector(1.0, 0.0, 0.0));
}

void WifiSimulation::SetupApplications() {
    uint16_t port = 9;
    Address sinkAddress(InetSocketAddress(m_staInterface.GetAddress(0), port));
    PacketSinkHelper packetSinkHelper("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApps = packetSinkHelper.Install(m_nodes.Get(1));
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(20.0));

    OnOffHelper onoff("ns3::UdpSocketFactory", sinkAddress);
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff.SetAttribute("DataRate", DataRateValue(DataRate("54Mbps")));
    onoff.SetAttribute("PacketSize", UintegerValue(1472));

    ApplicationContainer srcApps = onoff.Install(m_nodes.Get(0));
    srcApps.Start(Seconds(0.0));
    srcApps.Stop(Seconds(20.0));

    Gnuplot throughputPlot = Gnuplot(m_throughputFile);
    throughputPlot.SetTitle("Throughput vs Distance");
    throughputPlot.SetXlabel("Distance (m)");
    throughputPlot.SetYlabel("Throughput (Mbps)");

    Gnuplot2dDataset dataset;
    dataset.SetTitle("Average Throughput");
    dataset.SetStyle(Gnuplot2dDataset::LINES_POINTS);

    Ptr<PacketSink> sink = DynamicCast<PacketSink>(sinkApps.Get(0));
    for (double t = 1.0; t <= 20.0; t += 1.0) {
        double distance = t - 1.0;
        double bytes = sink->GetTotalRx() / (1024.0 * 1024.0); // MB
        double throughput = bytes / t * 8.0;                   // Mbps
        dataset.Add(distance, throughput);
    }

    throughputPlot.AddDataset(dataset);

    std::ofstream plotfile((m_throughputFile + ".plt").c_str());
    throughputPlot.GenerateOutput(plotfile);
    plotfile.close();
}

void WifiSimulation::LogRateAndPower(Ptr<OutputStreamWrapper> stream, Time interval) {
    if (Simulator::Now() >= Simulator::StopTime()) {
        return;
    }

    Ptr<WifiNetDevice> staDev = DynamicCast<WifiNetDevice>(m_staDevice.Get(0));
    Ptr<WifiPhy> phy = staDev->GetPhy(0);
    double txPower = phy->GetPowerDbm();
    DataRate dataRate = phy->GetDataRate();

    double distance = m_position;
    *stream->GetStream() << Simulator::Now().GetSeconds() << "\t"
                         << distance << "\t"
                         << txPower << "\t"
                         << dataRate.GetBitrate() / 1e6 << std::endl;

    Simulator::Schedule(interval, &WifiSimulation::LogRateAndPower, this, stream, interval);
}

void WifiSimulation::UpdatePosition() {
    if (Simulator::Now() >= Simulator::StopTime()) {
        return;
    }

    m_position++;
    Vector pos = m_nodes.Get(1)->GetObject<MobilityModel>()->GetPosition();
    pos.x = m_position;
    m_nodes.Get(1)->GetObject<ConstantVelocityMobilityModel>()->SetPosition(pos);

    Simulator::Schedule(Seconds(1.0), &WifiSimulation::UpdatePosition, this);
}

int main(int argc, char* argv[]) {
    std::string rateManager = "ParfWifiManager";
    uint32_t rtsThreshold = 2346;
    std::string throughputFile = "throughput.dat";
    std::string powerFile = "power.dat";

    CommandLine cmd(__FILE__);
    cmd.AddValue("rateManager", "WiFi Rate Control Manager (ParfWifiManager, AparfWifiManager, RrpaaWifiManager)", rateManager);
    cmd.AddValue("rtsThreshold", "RTS Threshold", rtsThreshold);
    cmd.AddValue("throughputFile", "Throughput output file name", throughputFile);
    cmd.AddValue("powerFile", "Transmit power output file name", powerFile);
    cmd.Parse(argc, argv);

    WifiSimulation simulation(rateManager, rtsThreshold, throughputFile, powerFile);
    simulation.Run();

    return 0;
}