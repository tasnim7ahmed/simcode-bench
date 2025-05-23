#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/onoff-helper.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/gnuplot.h"
#include "ns3/command-line-helper.h"
#include "ns3/log.h"
#include "ns3/constant-velocity-mobility-model.h"

#include <string>
#include <vector>
#include <map>
#include <fstream>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE("AdhocWifiExperiment");

class Experiment
{
public:
    Experiment(std::string wifiManager, std::string phyMode, int expNum);
    ~Experiment();

    void Run(double simulationTime);

private:
    void SetPosition(Ptr<Node> node, Vector position);
    Vector GetPosition(Ptr<Node> node);
    void SetVelocity(Ptr<Node> node, Vector velocity);

    void ReceivePacket(Ptr<Socket> socket);
    void SetupPacketReceive(Ptr<Socket> socket, Ptr<Node> node);
    void CalculateThroughput();

    NodeContainer m_nodes;
    NetDeviceContainer m_devices;
    
    std::string m_wifiManager;
    std::string m_phyMode;
    int m_experimentNumber;

    Ptr<OutputStreamWrapper> m_throughputStream;
    Gnuplot m_plot;
    GnuplotDataset m_dataset;

    uint64_t m_bytesTotal;
    Time m_prevTime;
    Time m_throughputMeasurementInterval;

    Ptr<ConstantVelocityMobilityModel> m_node0Mobility;
    Ptr<ConstantVelocityMobilityModel> m_node1Mobility;
};

Experiment::Experiment(std::string wifiManager, std::string phyMode, int expNum)
    : m_wifiManager(wifiManager),
      m_phyMode(phyMode),
      m_experimentNumber(expNum),
      m_bytesTotal(0),
      m_prevTime(Seconds(0.0)),
      m_throughputMeasurementInterval(Seconds(0.5))
{
    LogComponentEnable("AdhocWifiExperiment", LOG_LEVEL_INFO);
    LogComponentEnable("OnOffApplication", LOG_LEVEL_WARN);
    LogComponentEnable("PacketSink", LOG_LEVEL_WARN);
    LogComponentEnable("WifiPhy", LOG_LEVEL_INFO);

    std::string plotFilename = "adhoc-wifi-throughput-" + m_wifiManager + "-" + m_phyMode + "-" + std::to_string(m_experimentNumber) + ".plt";
    m_plot = Gnuplot(plotFilename);
    m_plot.SetTitle("Ad-hoc WiFi Throughput vs. Time");
    m_plot.SetXLabel("Time (s)");
    m_plot.SetYLabel("Throughput (kbps)");

    m_dataset.SetTitle("Throughput Data");
    m_dataset.SetStyle(GnuplotDataset::LINES_POINTS);
}

Experiment::~Experiment()
{
}

void
Experiment::SetPosition(Ptr<Node> node, Vector position)
{
    Ptr<ConstantVelocityMobilityModel> mobility = node->GetObject<ConstantVelocityMobilityModel>();
    if (mobility)
    {
        mobility->SetPosition(position);
    }
    else
    {
        NS_LOG_WARN("Node does not have ConstantVelocityMobilityModel.");
    }
}

Vector
Experiment::GetPosition(Ptr<Node> node)
{
    Ptr<ConstantVelocityMobilityModel> mobility = node->GetObject<ConstantVelocityMobilityModel>();
    if (mobility)
    {
        return mobility->GetPosition();
    }
    NS_LOG_WARN("Node does not have ConstantVelocityMobilityModel, returning (0,0,0).");
    return Vector(0,0,0);
}

void
Experiment::SetVelocity(Ptr<Node> node, Vector velocity)
{
    Ptr<ConstantVelocityMobilityModel> mobility = node->GetObject<ConstantVelocityMobilityModel>();
    if (mobility)
    {
        mobility->SetVelocity(velocity);
    }
    else
    {
        NS_LOG_WARN("Node does not have ConstantVelocityMobilityModel.");
    }
}

void
Experiment::ReceivePacket(Ptr<Socket> socket)
{
    Ptr<Packet> packet;
    Address from;
    while ((packet = socket->RecvFrom(from)))
    {
        m_bytesTotal += packet->GetSize();
    }
}

void
Experiment::SetupPacketReceive(Ptr<Socket> socket, Ptr<Node> node)
{
    socket->SetRecvCallback(MakeCallback(&Experiment::ReceivePacket, this));
}

void
Experiment::CalculateThroughput()
{
    double now = Simulator::Now().GetSeconds();
    if (now > m_prevTime.GetSeconds())
    {
        double currentThroughput = (m_bytesTotal * 8.0) / (now - m_prevTime.GetSeconds()) / 1024.0;
        m_dataset.Add(now, currentThroughput);
        *m_throughputStream << now << " " << currentThroughput << std::endl;
    }
    
    m_bytesTotal = 0;
    m_prevTime = Simulator::Now();

    Simulator::Schedule(m_throughputMeasurementInterval, &Experiment::CalculateThroughput, this);
}

void
Experiment::Run(double simulationTime)
{
    NS_LOG_INFO("Starting experiment with Manager: " << m_wifiManager << ", PhyMode: " << m_phyMode);

    Simulator::Destroy();

    m_bytesTotal = 0;
    m_prevTime = Seconds(0.0);
    m_dataset.Clear();

    m_nodes.Create(2);
    Ptr<Node> n0 = m_nodes.Get(0);
    Ptr<Node> n1 = m_nodes.Get(1);

    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.Install(m_nodes);

    m_node0Mobility = n0->GetObject<ConstantVelocityMobilityModel>();
    m_node1Mobility = n1->GetObject<ConstantVelocityMobilityModel>();

    SetPosition(n0, Vector(0, 0, 0));
    SetVelocity(n0, Vector(0, 0, 0));
    SetPosition(n1, Vector(100, 0, 0));
    SetVelocity(n1, Vector(-5, 0, 0));

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

    YansWifiPhyHelper wifiPhy;
    wifiPhy.Set("TxPowerStart", DoubleValue(8.0));
    wifiPhy.Set("TxPowerEnd", DoubleValue(8.0));
    wifiPhy.Set("RxGain", DoubleValue(0.0));
    wifiPhy.Set("EnergyDetectionThreshold", DoubleValue(-90.0));
    wifiPhy.Set("CcaMode1Threshold", DoubleValue(-93.0));

    YansWifiChannelHelper wifiChannel;
    wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    wifiChannel.AddPropagationLoss("ns3::FriisPropagationLossModel");
    wifiPhy.SetChannel(wifiChannel.Create());

    NqosWifiMacHelper wifiMac = NqosWifiMacHelper::Default();
    wifiMac.SetType("ns3::AdhocWifiMac");

    if (m_wifiManager == "ns3::ConstantRateWifiManager")
    {
        wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                     "DataMode", StringValue(m_phyMode),
                                     "ControlMode", StringValue(m_phyMode));
    }
    else
    {
        wifi.SetRemoteStationManager(m_wifiManager);
    }
    wifiPhy.Set("PhyMode", StringValue(m_phyMode));

    m_devices = wifi.Install(wifiPhy, wifiMac, m_nodes);

    InternetStackHelper internet;
    internet.Install(m_nodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(m_devices);

    uint16_t sinkPort = 9;
    Address sinkAddress (InetSocketAddress(interfaces.GetAddress(1), sinkPort));

    PacketSinkHelper packetSinkHelper("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sinkApps = packetSinkHelper.Install(n1);
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(simulationTime + 0.1));

    OnOffHelper onoffHelper("ns3::UdpSocketFactory", sinkAddress);
    onoffHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoffHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoffHelper.SetAttribute("PacketSize", UintegerValue(1024));
    onoffHelper.SetAttribute("DataRate", DataRateValue(DataRate("500kbps")));

    ApplicationContainer sourceApps = onoffHelper.Install(n0);
    sourceApps.Start(Seconds(1.0));
    sourceApps.Stop(Seconds(simulationTime));

    std::string throughputLogFile = "throughput-" + m_wifiManager + "-" + m_phyMode + "-" + std::to_string(m_experimentNumber) + ".txt";
    m_throughputStream = Create<OutputStreamWrapper>(throughputLogFile, std::ios::out);
    *m_throughputStream << "# Time (s)\tThroughput (kbps)" << std::endl;

    Simulator::Schedule(Seconds(0.1), &Experiment::CalculateThroughput, this);

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    std::ofstream plotFile(m_plot.GetFilename().c_str());
    m_plot.GenerateOutput(plotFile);
    plotFile.close();

    NS_LOG_INFO("Experiment finished for Manager: " << m_wifiManager << ", PhyMode: " << m_phyMode);
}

} // namespace ns3

int main(int argc, char *argv[])
{
    double simulationTime = 20.0;

    CommandLine cmd(__FILE__);
    cmd.AddValue("simulationTime", "Total duration of the simulation", simulationTime);
    cmd.Parse(argc, argv);

    std::vector<std::string> wifiManagers;
    wifiManagers.push_back("ns3::AarfWifiManager");
    wifiManagers.push_back("ns3::ConstantRateWifiManager");
    wifiManagers.push_back("ns3::IdealWifiManager");

    std::vector<std::string> phyModes;
    phyModes.push_back("DsssRate1Mbps");
    phyModes.push_back("DsssRate2Mbps");
    phyModes.push_back("DsssRate5_5Mbps");
    phyModes.push_back("DsssRate11Mbps");

    int experimentCounter = 0;

    for (const auto& manager : wifiManagers)
    {
        for (const auto& mode : phyModes)
        {
            experimentCounter++;
            ns3::Experiment experiment(manager, mode, experimentCounter);
            experiment.Run(simulationTime);
        }
    }

    return 0;
}