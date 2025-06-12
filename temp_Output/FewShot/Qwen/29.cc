#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/gnuplot.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MultiRateWifiExperiment");

class MultiRateWifiExperiment {
public:
    MultiRateWifiExperiment();
    void Run(uint32_t nNodes, uint32_t nFlows, double simulationTime);
    void SetupApplications();
    void SetupMobility();
    void CalculateThroughput();
    void PacketSinkRx(Ptr<const Packet> p, const Address &ad);

private:
    NodeContainer m_nodes;
    NetDeviceContainer m_devices;
    Ipv4InterfaceContainer m_interfaces;
    std::vector<ApplicationContainer> m_clients;
    std::vector<ApplicationContainer> m_servers;
    std::map<Ptr<Socket>, uint64_t> m_receivedPackets;
    Gnuplot2dDataset m_throughputDataset;
    double m_lastCalculationTime;
};

MultiRateWifiExperiment::MultiRateWifiExperiment()
    : m_lastCalculationTime(0.0)
{
    m_throughputDataset.SetStyle(Gnuplot2dDataset::LINES_POINTS);
}

void MultiRateWifiExperiment::Run(uint32_t nNodes, uint32_t nFlows, double simulationTime) {
    NS_LOG_INFO("Creating nodes.");
    m_nodes.Create(nNodes);

    NS_LOG_INFO("Setting up WiFi.");
    WifiMacHelper wifiMac;
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211a);
    wifi.SetRemoteStationManager("ns3::AarfcdWifiManager");

    wifiMac.SetType("ns3::AdhocWifiMac");
    m_devices = wifi.Install(wifiMac, m_nodes);

    NS_LOG_INFO("Installing internet stack.");
    InternetStackHelper stack;
    stack.Install(m_nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.0.0.0", "255.255.255.0");
    m_interfaces = address.Assign(m_devices);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    SetupMobility();

    NS_LOG_INFO("Setting up applications.");
    SetupApplications();

    NS_LOG_INFO("Configuring packet sink callbacks for throughput monitoring.");
    Config::Connect("/NodeList/*/ApplicationList/*/$ns3::PacketSink/Rx",
                    MakeCallback(&MultiRateWifiExperiment::PacketSinkRx, this));

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();
    Simulator::Destroy();
}

void MultiRateWifiExperiment::SetupApplications() {
    uint16_t port = 9;

    for (uint32_t i = 0; i < m_clients.size(); ++i) {
        m_servers.at(i).Stop(Seconds(0.1));
        m_clients.at(i).Stop(Seconds(0.1));
    }

    m_servers.clear();
    m_clients.clear();

    for (uint32_t i = 0; i < m_nodes.GetN(); ++i) {
        for (uint32_t j = 0; j < m_nodes.GetN(); ++j) {
            if (i != j) {
                PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
                ApplicationContainer serverApp = sink.Install(m_nodes.Get(j));
                serverApp.Start(Seconds(1.0));
                serverApp.Stop(Seconds(100.0));
                m_servers.push_back(serverApp);

                OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(m_interfaces.GetAddress(j), port));
                onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
                onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
                onoff.SetAttribute("DataRate", DataRateValue(DataRate("1Mbps")));
                onoff.SetAttribute("PacketSize", UintegerValue(1024));

                ApplicationContainer clientApp = onoff.Install(m_nodes.Get(i));
                clientApp.Start(Seconds(1.0));
                clientApp.Stop(Seconds(100.0));
                m_clients.push_back(clientApp);
            }
        }
    }
}

void MultiRateWifiExperiment::SetupMobility() {
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(10.0),
                                  "DeltaY", DoubleValue(10.0),
                                  "GridWidth", UintegerValue(10),
                                  "LayoutType", StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(0, 1000, 0, 1000)));
    mobility.Install(m_nodes);
}

void MultiRateWifiExperiment::PacketSinkRx(Ptr<const Packet> p, const Address &ad) {
    m_receivedPackets[Simulator::GetContext()] += p->GetSize();
}

void MultiRateWifiExperiment::CalculateThroughput() {
    double now = Simulator::Now().GetSeconds();
    double interval = now - m_lastCalculationTime;
    m_lastCalculationTime = now;

    uint64_t totalBytes = 0;
    for (const auto& pair : m_receivedPackets) {
        totalBytes += pair.second;
    }

    double throughput = (totalBytes * 8.0 / interval) / 1e6; // Mbps
    m_throughputDataset.Add(now, throughput);
    NS_LOG_INFO("Throughput: " << throughput << " Mbps");

    m_receivedPackets.clear();

    Simulator::Schedule(Seconds(1.0), &MultiRateWifiExperiment::CalculateThroughput, this);
}

int main(int argc, char *argv[]) {
    uint32_t nNodes = 100;
    uint32_t nFlows = 10;
    double simulationTime = 20.0;

    CommandLine cmd(__FILE__);
    cmd.AddValue("nNodes", "Number of nodes in the network.", nNodes);
    cmd.AddValue("nFlows", "Number of simultaneous flows.", nFlows);
    cmd.AddValue("simulationTime", "Duration of the simulation in seconds.", simulationTime);
    cmd.Parse(argc, argv);

    LogComponentEnable("MultiRateWifiExperiment", LOG_LEVEL_INFO);
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);

    MultiRateWifiExperiment experiment;
    experiment.Run(nNodes, nFlows, simulationTime);

    NS_LOG_INFO("Generating Gnuplot output.");
    Gnuplot gnuplot("throughput.png");
    gnuplot.SetTitle("Throughput over Time");
    gnuplot.SetXLabel("Time (s)");
    gnuplot.SetYLabel("Throughput (Mbps)");
    gnuplot.AddDataset(experiment.m_throughputDataset);
    gnuplot.GenerateOutput(std::cout);

    return 0;
}