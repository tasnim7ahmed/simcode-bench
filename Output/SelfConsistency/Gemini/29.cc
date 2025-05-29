#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/gnuplot.h"
#include "ns3/olsr-helper.h"
#include "ns3/netanim-module.h"

#include <iostream>
#include <fstream>
#include <vector>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiMultirateExperiment");

// Forward declaration of the WifiMultirateExperiment class
class WifiMultirateExperiment;

// Global pointer to the WifiMultirateExperiment instance
WifiMultirateExperiment* g_experiment = nullptr;


class WifiMultirateExperiment {
public:
    WifiMultirateExperiment();
    ~WifiMultirateExperiment();

    void Run(int argc, char* argv[]);

private:
    void CreateNodes();
    void InstallWifi();
    void InstallInternetStack();
    void InstallApplications();
    void SetupMobility();
    void MonitorPacketReception();
    void CalculateThroughput();
    void CreateGnuplot();

    // Configuration parameters
    uint32_t m_numNodes = 100;
    double m_simulationTime = 10.0;
    std::string m_rate = "DsssRate11Mbps"; // Default data rate
    std::string m_phyMode = "DsssRate11Mbps";
    std::string m_channelWidth = "20"; // 20 MHz channel width
    double m_nodeSpeed = 1.0; // m/s
    double m_pauseTime = 0.0;

    NodeContainer m_nodes;
    NetDeviceContainer m_devices;
    Ipv4InterfaceContainer m_interfaces;
    ApplicationContainer m_sinkApps;
    ApplicationContainer m_clientApps;

    FlowMonitorHelper m_flowMonitor;
    Ptr<FlowMonitor> m_flowmon;

    std::string m_prefixGnuplot;
    std::string m_gnuplotTitle;

    // Callback for packet reception
    static void ReceivePacket(Ptr<const Packet> packet, const Address& fromAddress);

    // Callback for end of simulation
    void OnSimulationEnd();
};


WifiMultirateExperiment::WifiMultirateExperiment() :
    m_prefixGnuplot("wifi-multirate"),
    m_gnuplotTitle("Wifi Multirate Experiment") {
        g_experiment = this;
    }

WifiMultirateExperiment::~WifiMultirateExperiment() {
    g_experiment = nullptr;
}


void WifiMultirateExperiment::Run(int argc, char* argv[]) {
    CommandLine cmd;
    cmd.AddValue("numNodes", "Number of nodes", m_numNodes);
    cmd.AddValue("simulationTime", "Simulation time (s)", m_simulationTime);
    cmd.AddValue("rate", "Wifi data rate", m_rate);
    cmd.AddValue("phyMode", "Wifi PHY mode", m_phyMode);
    cmd.AddValue("channelWidth", "Channel width (MHz)", m_channelWidth);
    cmd.Parse(argc, argv);

    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    CreateNodes();
    InstallWifi();
    InstallInternetStack();
    InstallApplications();
    SetupMobility();
    MonitorPacketReception();

    Simulator::Stop(Seconds(m_simulationTime));

    // Schedule the throughput calculation at the end of the simulation
    Simulator::Schedule(Seconds(m_simulationTime), &WifiMultirateExperiment::CalculateThroughput, this);
    Simulator::Schedule(Seconds(m_simulationTime), &WifiMultirateExperiment::CreateGnuplot, this);

    Simulator::Run();
    Simulator::Destroy();
}


void WifiMultirateExperiment::CreateNodes() {
    NS_LOG_INFO("Creating " << m_numNodes << " nodes.");
    m_nodes.Create(m_numNodes);
}


void WifiMultirateExperiment::InstallWifi() {
    NS_LOG_INFO("Installing Wifi.");

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Create();
    wifiPhy.SetChannel(wifiChannel.Create());

    // Add a mac and disable rate control
    WifiMacHelper wifiMac;
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode", StringValue(m_rate),
                                 "ControlMode", StringValue(m_rate));

    wifiMac.SetType("ns3::AdhocWifiMac");
    m_devices = wifi.Install(wifiPhy, wifiMac, m_nodes);

}


void WifiMultirateExperiment::InstallInternetStack() {
    NS_LOG_INFO("Installing Internet Stack.");
    OlsrHelper olsr;
    InternetStackHelper internet;
    internet.SetRoutingHelper(olsr);
    internet.Install(m_nodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    m_interfaces = ipv4.Assign(m_devices);
}


void WifiMultirateExperiment::InstallApplications() {
    NS_LOG_INFO("Installing Applications.");

    uint16_t sinkPort = 8080;
    UdpServerHelper sink(sinkPort);

    // Install a sink app on node 0
    m_sinkApps = sink.Install(m_nodes.Get(0));
    m_sinkApps.Start(Seconds(1.0));
    m_sinkApps.Stop(Seconds(m_simulationTime - 1.0));

    UdpClientHelper client(m_interfaces.GetAddress(0), sinkPort);
    client.SetAttribute("MaxPackets", UintegerValue(4294967295));
    client.SetAttribute("Interval", TimeValue(Time("0.001"))); // packets/sec
    client.SetAttribute("PacketSize", UintegerValue(1024));

    // Install client app on remaining nodes
    m_clientApps = client.Install(m_nodes.Get(1), m_nodes.Get(2), m_nodes.Get(3), m_nodes.Get(4)); // just a few clients for demonstration
    m_clientApps.Start(Seconds(2.0));
    m_clientApps.Stop(Seconds(m_simulationTime - 2.0));
}


void WifiMultirateExperiment::SetupMobility() {
    NS_LOG_INFO("Setting up Mobility.");

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::RandomDiscPositionAllocator",
                                  "X", StringValue("100.0"),
                                  "Y", StringValue("100.0"),
                                  "Rho", StringValue("ns3::UniformRandomVariable[Min=0|Max=100]"));

    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                               "Mode", StringValue("Time"),
                               "Time", StringValue("1s"),
                               "Speed", StringValue("ns3::ConstantRandomVariable[Constant=" + std::to_string(m_nodeSpeed) + "]"),
                               "Bounds", StringValue("100x100"));
    mobility.Install(m_nodes);

    // Pause at start
    for (uint32_t i = 0; i < m_nodes.GetN(); ++i) {
        Ptr<Node> node = m_nodes.Get(i);
        Ptr<MobilityModel> mobilityModel = node->GetObject<MobilityModel>();
        mobilityModel->SetPosition(Vector(i * 1.0, i * 1.0, 0.0));
    }

}


void WifiMultirateExperiment::MonitorPacketReception() {
    NS_LOG_INFO("Monitoring Packet Reception.");
    for (uint32_t i = 0; i < m_numNodes; ++i) {
        Ptr<Node> node = m_nodes.Get(i);
        Ptr<NetDevice> device = m_devices.Get(i);

        // Connect the trace source to a callback function
        device->TraceConnectWithoutContext("MonitorSnifferRx", MakeCallback(&WifiMultirateExperiment::ReceivePacket));
    }
}


void WifiMultirateExperiment::ReceivePacket(Ptr<const Packet> packet, const Address& fromAddress) {
    // This function is called whenever a packet is received by any of the nodes.
    // You can add custom processing logic here, such as logging packet information.

    // Convert the Address to an Ipv4Address if possible
    Ipv4Address ipv4Address;
    if (InetSocketAddress::IsMatchingType(fromAddress)) {
        InetSocketAddress inetSocketAddress = InetSocketAddress::ConvertFrom(fromAddress);
        ipv4Address = inetSocketAddress.GetIpv4();
    }

    NS_LOG_DEBUG("Received packet: Size=" << packet->GetSize() << ", From=" << ipv4Address);
}


void WifiMultirateExperiment::CalculateThroughput() {
    NS_LOG_INFO("Calculating Throughput.");

    m_flowmon = m_flowMonitor.FindFlowMonitor(1);

    double totalPacketsReceived = 0;
    double totalBytesReceived = 0;

    Time firstTxTime = Seconds(m_simulationTime);
    Time lastRxTime = Seconds(0);


    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = m_flowmon->GetFlowStats().begin(); i != m_flowmon->GetFlowStats().end(); ++i) {
        totalPacketsReceived += i->second.rxPackets;
        totalBytesReceived += i->second.rxBytes;

        if (i->second.timeFirstTxPacket < firstTxTime) {
          firstTxTime = i->second.timeFirstTxPacket;
        }
        if (i->second.timeLastRxPacket > lastRxTime) {
          lastRxTime = i->second.timeLastRxPacket;
        }
    }

    double duration = lastRxTime.GetSeconds() - firstTxTime.GetSeconds();

    double throughput = (totalBytesReceived * 8) / (duration * 1000000.0); // Mbps
    std::cout << "Total Packets Received: " << totalPacketsReceived << std::endl;
    std::cout << "Total Bytes Received: " << totalBytesReceived << std::endl;
    std::cout << "Duration: " << duration << " s" << std::endl;
    std::cout << "Average Throughput: " << throughput << " Mbps" << std::endl;

}


void WifiMultirateExperiment::CreateGnuplot() {
    NS_LOG_INFO("Creating Gnuplot.");

    std::string plotFileName = m_prefixGnuplot + ".plt";
    std::string dataFileName = m_prefixGnuplot + ".dat";

    Gnuplot plot(plotFileName);
    plot.SetTitle(m_gnuplotTitle);
    plot.SetTerminal("png");
    plot.SetLegend("Time (s)", "Throughput (Mbps)");

    std::ofstream dataFile(dataFileName.c_str());

    if (!dataFile.is_open()) {
        std::cerr << "Error: Could not open data file for Gnuplot: " << dataFileName << std::endl;
        return;
    }

    // Dummy data for demonstration purposes.  Replace with actual data.
    for (double time = 0.0; time <= m_simulationTime; time += 1.0) {
        dataFile << time << " " << 1.0 << std::endl; // Example throughput value
    }

    dataFile.close();

    plot.AddDataset(dataFileName, "Throughput");
    plot.GenerateOutput(std::cout); // Print the plot commands to stdout

}



int main(int argc, char* argv[]) {
    LogComponentEnable("WifiMultirateExperiment", LOG_LEVEL_INFO);
    WifiMultirateExperiment experiment;
    experiment.Run(argc, argv);

    return 0;
}